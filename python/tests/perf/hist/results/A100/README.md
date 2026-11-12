# A100 results for PixelHistogramCUDA (Merlin7)

NVIDIA A100-SXM4-80GB (40 MB L2 cache) on Merlin7 (gmerlin7, a100 partitions),
CUDA 12.9.1, nsys 2025.1.3 with CUDA event trace off. Data: raw MÖNCH frames
(Cu fluorescence, 400 × 400 pixels, uint16, no pedestal subtracted). Driver and
git state are in each run's `env.json`.

| Folder | Accumulator layout | Frames | What was run |
|---|---|---|---|
| `2026-10-29_bin-major` | bin-major | 20 000 | the 4 configurations of `run_probes.py` (nsys) |
| `2026-11-05_cpu_vs_gpu` | both | 20 000 | `cpu_vs_gpu.py`, direct kernel only |
| `2026-11-11_window` | both | 20 000 | `cpu_vs_gpu.py` and ncu, window and direct kernels, batch size 1024; nsys sweeps (pixel-major) in `pixelMajor/nsys` and `pixelMajor/nsys_batch` |

## Terms used below

- **Fill**: everything `fill_histogram()` does. It copies a batch of frames
  from host memory to the GPU and runs the fill kernel on it. The copy of the
  next batch overlaps with the kernel of the current one, so the fill runs at
  the speed of the slower of the two.
- **Copy limit**: the time to copy one frame over PCIe, about 26 GB/s here:
  12 µs per frame for uint16 frames, 24 µs for float32, 48 µs for float64.
  A fill at this limit is **copy-bound**: a faster kernel no longer helps.
- **Readout (`values()`)**: reads the histogram back. In these runs a small
  GPU kernel capped the counts (and in the bin-major layout transposed them),
  then the whole histogram was copied from the GPU to host memory. Since
  12 Nov, with `uint32` counts (the default, used in all these runs) there is
  nothing to cap and `values()` copies the histogram directly, without a
  kernel; other count types still run the cap kernel (`cap_counts`).
- **Accumulator layout**: how the histogram is stored in GPU memory.
  **Bin-major**: `[bin][pixel]`, one bin of all pixels after another.
  **Pixel-major**: `[pixel][bin]`, one pixel's whole histogram after another;
  this is the order `values()` returns, so no transpose is needed.
  The runs below compare both. Pixel-major won (see the decision below) and
  is now the only layout in the code; the bin-major runs were made with a
  compile-time layout switch that has since been removed (last present in
  commit `c7fa15e`).
- **Direct kernel** (`fill_direct`, named `fill_bin_major` when these runs
  were made): every value increments its bin directly in GPU memory.
- **Window kernel** (`fill_window`, the default since 11 Nov): each pixel
  counts 128 consecutive bins around its noise peak (the "window") in fast
  on-chip shared memory during a batch. Values outside the window (mostly photons) go straight to
  GPU memory. At the end of the batch the window counts are added to the
  histogram in GPU memory (the "flush"). The window places itself on the
  first batch and follows the pixel's pedestal afterwards.
- **ncu** times the GPU kernel alone; `cpu_vs_gpu.py` times the whole fill and
  readout from Python.

## nsys baseline, direct kernel, bin-major (`2026-10-29_bin-major`)

Configuration names read `<input type>_b<bins>_bs<batch size>`. Range taken
from the data, [4171, 5744) ADU.

| Configuration | Fill kernel (µs/frame) | Host-to-GPU copy (µs/frame) | Limited by | Frames per second | Readout kernel (ms) | GPU-to-host copy of the histogram (ms) |
|---|--:|--:|---|--:|--:|--:|
| `u16_b1000_bs64` | 23.3 | 12.0 | kernel | 42 900 | 5.8 | 24.3 |
| `f32_b1000_bs64` | 23.5 | 24.0 | copy and kernel equally | 41 700 | 5.8 | 24.3 |
| `u16_b100_bs64` | 4.4 | 12.0 | copy | 83 100 | 0.4 | 2.4 |
| `u16_b1000_bs512` | 22.9 | 11.7 | kernel | 43 600 | 5.8 | 24.4 |

At 1000 bins the kernel is the limit, so uint16 input gains nothing over
float32 yet, and the batch size changes less than 2 %.

## Direct kernel vs binning and layout (ncu, not stored as files)

The direct kernel alone under ncu, batch of 64 frames, in µs per frame, with
the L2 cache hit rate in brackets. "Centred" frames have each pixel's median
subtracted and stay uint16 (`--center 1000`, range [937, 2127)), so
neighbouring pixels fall in the same bins.

| Bins | Bin width, raw frames (ADU) | Raw, bin-major | Raw, pixel-major | Bin width, centred frames (ADU) | Centred, bin-major | Centred, pixel-major |
|--:|--:|--:|--:|--:|--:|--:|
| 100 | 15.7 | **4.5** (88 %) | 7.5 (89 %) | 11.9 | **2.6** (85 %) | 7.6 (89 %) |
| 200 | 7.9 | 9.4 (77 %) | **8.9** (86 %) | 6.0 | **3.6** (86 %) | 9.2 (86 %) |
| 500 | 3.1 | 19.1 (65 %) | **12.0** (80 %) | 2.4 | **6.0** (83 %) | 13.7 (77 %) |
| 1000 | 1.6 | 23.1 (62 %) | **16.1** (73 %) | 1.2 | **11.9** (71 %) | 18.8 (68 %) |
| 2000 | 0.8 | 26.6 (60 %) | **21.2** (65 %) | 0.6 | **15.0** (68 %) | 22.3 (64 %) |

1000 bins over a 10× wider range (`--range-scale 10`: 15.7 ADU per bin, a
640 MB histogram) gives 4.5 µs/frame and 88 %, the same as 100 bins over the
normal range (a 64 MB histogram).

- The kernel's cost follows how many distinct (bin, pixel) cells the frames
  hit, which the bin width sets. The size of the histogram doesn't matter.
- Raw frames in bin-major are limited by GPU memory bandwidth from 200 bins
  up, at about 620 GB/s: from 1000 bins on, nearly every increment fetches
  64 B from GPU memory.
- Bin-major is 2–4× faster when neighbouring pixels share bins (centred
  frames). Pixel-major helps raw frames from 200 bins up, but stays above
  about 7.5 µs/frame, because each warp's 32 increments touch 32 different
  32-byte memory sectors.
- Under the 12 µs/frame uint16 copy limit: raw bin-major up to about 280
  bins, raw pixel-major up to about 500, centred bin-major up to about 1000.
  No layout keeps raw uint16 frames copy-bound at 1000 bins.

To reproduce, from `python/tests/perf/hist` (the bin-major columns need
commit `c7fa15e` or earlier, with the layout switch set to bin-major):

```bash
M="--cache-control none --kernel-name regex:fill_ --launch-skip 20 --launch-count 5 --print-summary per-kernel --metrics gpu__time_duration.avg,lts__t_sector_hit_rate.pct,dram__bytes_read.sum,dram__bytes_write.sum"
for b in 100 200 500 1000 2000; do
  ncu $M python nsys_hist_probe.py u16 2000 $b 64 2000 2>&1 | grep -E "^dtype|duration|hit_rate|dram__"
done
# add --center 1000 for centred frames, or use --range-scale 10 at 1000 bins
```

Divide the duration by 64 to get µs/frame.

## Direct kernel vs the CPU, both layouts (`2026-11-05_cpu_vs_gpu`)

`cpu_vs_gpu.py` on gpu104, before the window kernel: 20 000 raw frames, range
[3500, 7500) ADU, batch size 128, direct kernel, one folder per layout
(`binMajor_kernel/`, `pixelMajor_kernel/`, each with `runs.csv` and plots).
Every GPU run gave the same histogram as the CPU. Fill of uint16 frames, in
µs per frame (copy limit 12.0):

| Bin width (ADU) | Bins | Bin-major layout | Pixel-major layout |
|--:|--:|--:|--:|
| 16 | 250 | 12.4 | 12.4 |
| 8 | 500 | 12.3 | 12.4 |
| 4 | 1000 | 17.2 | 12.3 |
| 2 | 2000 | 22.8 | 14.6 |
| 1 | 4000 | 26.1 | 19.9 |

float32 and float64 frames fill at their copy limit (24 and 48 µs per frame)
at every width in pixel-major. With the direct kernel, uint16 frames stay
copy-bound down to 4 ADU per bin in pixel-major and 8 ADU in bin-major; the
window kernel (next section) removes that limit.

## Window kernel vs direct kernel, in both layouts (`2026-11-11_window`)

Batch size 1024 frames, 20 000 frames, node gpu103. One folder per layout
(`binMajor/`, `pixelMajor/`); each `runs.csv` holds both kernels (column
`gpu_kernel`). Every run gives a histogram identical to the CPU's.

### Kernel alone (ncu, `ncu.txt`)

Range taken from the data, [4171, 5744) ADU, so the bins are narrower than in
the CPU vs GPU runs further down. Times are per frame (kernel time for one
batch / 1024).

**Kernel time (µs per frame)**

| Bins | Bin width (ADU) | Window kernel, bin-major layout | Window kernel, pixel-major layout | Direct kernel, bin-major layout | Direct kernel, pixel-major layout |
|--:|--:|--:|--:|--:|--:|
| 250 | 6.3 | **3.0** | 3.4 | 11.5 | 9.0 |
| 2000 | 0.79 | **5.6** | 5.9 | 26.0 | 20.9 |
| 4000 | 0.39 | **7.7** | 7.8 | 25.8 | 24.4 |

**GPU memory traffic per batch of 1024 frames (GB read + written)**

| Bins | Window kernel, bin-major layout | Window kernel, pixel-major layout | Direct kernel, bin-major layout | Direct kernel, pixel-major layout |
|--:|--:|--:|--:|--:|
| 250 | 0.65 | 0.62 | 6.7 | 3.2 |
| 2000 | 2.1 | 1.3 | 16.0 | 12.1 |
| 4000 | 3.9 | 3.0 | 16.0 | 14.7 |

- The window kernel is 2.7 to 4.6× faster than the direct kernel, and stays
  well under the 12 µs/frame uint16 copy limit at every bin width.
- In the bin-major layout the window kernel is 2 to 12 % faster, although it
  moves more bytes: this kernel is not limited by GPU memory bandwidth.
- Its time rises with narrower bins; see "What sets the window kernel's
  speed" below.

### Whole fill and readout vs the CPU (`cpu_vs_gpu.py`, `runs.csv`)

Range [3500, 7500) ADU (4000 ADU). Fill time per frame with uint16 frames; the
copy limit is 12 µs/frame.

**Fill time (µs per frame), uint16 frames**

| Bins | Bin width (ADU) | Window kernel, bin-major layout | Window kernel, pixel-major layout | Direct kernel, bin-major layout | Direct kernel, pixel-major layout |
|--:|--:|--:|--:|--:|--:|
| 250 | 16 | 12.1 | 12.1 | 12.1 | 12.3 |
| 500 | 8 | 12.1 | 12.2 | 12.4 | 12.4 |
| 1000 | 4 | 12.1 | 12.1 | 17.1 | 12.5 |
| 2000 | 2 | 12.1 | 12.1 | 23.0 | 14.3 |
| 4000 | 1 | 12.2 | 12.2 | 26.5 | 20.4 |

**Readout time, one `values()` call (ms), window kernel**

| Bins | Histogram size (uint32 counts) | Bin-major layout | Pixel-major layout |
|--:|--:|--:|--:|
| 250 | 160 MB | 13 | 12 |
| 500 | 320 MB | 21 | 19 |
| 1000 | 640 MB | 37 | 32 |
| 2000 | 1.28 GB | 68 | 58 |
| 4000 | 2.56 GB | 131 | 111 |

- With the window kernel the fill is copy-bound at every bin width, for every
  input type and in both layouts: 12.2, 24.4 and 48.2 µs/frame for uint16,
  float32 and float64 frames.
- The pixel-major readout is about 15 % faster because it needs no
  transpose. The rest of the readout time is the copy of the histogram to
  host memory.
- One outlier: float32 frames, direct kernel, bin-major, 1000 bins
  (37.1 µs/frame, readout 0.23 s). The same point in the pixel-major run is
  24.4 µs/frame.

**Decision: pixel-major layout** (the default since 11 Nov). Wherever the
layout makes a difference a user would see, pixel-major is equal or better:
the window fill is the same, the readout is faster, and the direct kernel is
faster. The bin-major layout's small lead in the window kernel stays hidden
behind the copy.

Plots in each folder, made with
`plot_cpu_vs_gpu.py <folder>/runs.csv --kernel window` (or `direct`):
`throughput_vs_binwidth_window.png` and `speedup_vs_binwidth_window.png`
(and `_direct`).

### Copy or kernel? (nsys, `pixelMajor/nsys/`)

nsys records, in the same run, how long the copy engine and the fill kernel
are busy. They overlap, so the fill runs at the speed of the slower one.
Pixel-major layout, batch 1024, range [3500, 7500) ADU, 20 000 frames per
run. Plot: `fill_bound.png`.

**Busy time per frame (µs), 16 → 1 ADU per bin**

| Input frames | Copy in | Window kernel | Direct kernel |
|---|--:|--:|--:|
| uint16 | 12.0 | 2.9 → 5.6 | 7.7 → **20.1** |
| float32 | 24.0 | 2.8 → 5.4 | 7.6 → 20.2 |
| float64 | 48.0 | 2.9 → 5.5 | 7.8 → 20.6 |

- With the window kernel the copy limits the fill everywhere: the kernel
  takes at most half the uint16 copy time, at 1 ADU per bin.
- The direct kernel limits the uint16 fill from 2 ADU per bin down (fill
  14.6 and 20.7 µs/frame).
- The kernel time hardly depends on the input type: the work is per value,
  not per byte.
- The fill time per frame (first copy start to last kernel end) is within
  0.2 µs of the copy time: there are no gaps between batches.

### Batch size (nsys, `pixelMajor/nsys_batch/`)

uint16 frames, window kernel, 40 960 frames per run. The copy takes 12.0
µs/frame at every batch size. Plot: `batch_size.png`.

**Window kernel busy time per frame (µs)**

| Batch size | 4 ADU per bin | 2 ADU per bin | 1 ADU per bin | Fill time, 1 ADU per bin |
|--:|--:|--:|--:|--:|
| 64 | 7.5 | 9.9 | **11.9** | 12.3 |
| 128 | 5.8 | 7.3 | 9.1 | 12.3 |
| 256 | 4.8 | 6.0 | 7.1 | 12.3 |
| 512 | 4.3 | 5.2 | 6.0 | 12.3 |
| 1024 | 3.8 | 4.7 | 5.3 | 12.2 |
| 4096 | 4.0 | 4.3 | 4.6 | 12.5 |

- The batch size sets the kernel's margin under the copy, not the fill
  speed: the fill stays at the copy time for every batch size tested.
- At batch 64 (the Python default until 12 Nov) the kernel is busy as long as the
  copy at 1 ADU per bin: no margin left.
- From 1024 on the kernel stays under half the copy time; 4096 gains little
  more, and its fill is slightly slower (12.5 µs/frame) because the first
  copy and the last kernel, which don't overlap, are longer.
- Default since 12 Nov: 1024. It holds 2 × 1024 frames on the GPU: 655 MB for
  uint16 frames of 400 × 400, 2.6 GB for float64.

An earlier version of `run_probes.py` left the first batch's copy out of the
copy time (it ends before the first kernel starts), so the copy came out up
to 10 % low at large batches. These CSVs were recomputed with
`run_probes.py --reanalyze` and count every copy.

### What sets the window kernel's speed

The window kernel touches GPU memory in two cases only; everything else
happens in shared memory, which is much faster:

- a **miss**: a value outside its pixel's window is added to GPU memory
  directly, as the direct kernel does for every value;
- the **flush**: at the end of each batch, every non-zero window counter is
  added to the histogram in GPU memory.

So its cost follows the GPU memory updates per value, misses plus flush. The
direct kernel makes exactly 1 per value. Reading the frames themselves costs
the same in both kernels.

Three settings change that number. The figures below come from
`window_hits.py` (`../window_hits_2026-11-10/`), a numpy simulation of the
kernel's placement and counting on the same 20 000 MÖNCH frames, not from GPU
timings:

**GPU memory updates per value, window of 128 bins (simulated)**

| Bin width (ADU) | Batch 128 | Batch 512 | Batch 1024 |
|--:|--:|--:|--:|
| 16 | 0.10 | 0.05 | 0.04 |
| 8 | 0.14 | 0.07 | 0.05 |
| 4 | 0.22 | 0.10 | 0.07 |
| 2 | 0.35 | 0.14 | 0.10 |
| 1 | 0.52 | 0.22 | 0.15 |

**GPU memory updates per value, batch 1024, by window size (simulated)**

| Bin width (ADU) | Window 32 bins | Window 64 bins | Window 128 bins |
|--:|--:|--:|--:|
| 2 | 0.18 | 0.10 | 0.10 |
| 1 | 0.52 (37 % of windows placed again each batch) | 0.21 | 0.15 |

- **Bin width (measured).** The window always covers 128 bins, so narrower
  bins cover fewer ADU of the noise: 128 ADU at 1 ADU per bin, only 50 ADU at
  0.39 ADU per bin, about ±1.2 noise widths. More values miss, and more window
  counters are non-zero at the flush. Kernel time, bin-major: 3.0, 5.6 and
  7.7 µs/frame at 6.3, 0.79 and 0.39 ADU per bin.
- **Batch size (measured, see above).** The flush happens once per batch, so
  its cost per value falls roughly as 1 / batch size; the misses don't depend
  on it. Measured at 1 ADU per bin: kernel 11.9 µs/frame at batch 64, 5.3 at
  1024. Batch size is limited to 65 535 because the window counters are
  16-bit.
- **Window size (simulated only).** A wider window catches more values but
  has more counters to flush and needs more shared memory: 128 bins × 128
  pixels per block × 2 bytes = 32 KB per block, against a 48 KB default
  limit, so 256 bins would need an opt-in. At 1 ADU per bin a 32-bin window
  fails: the window keeps being placed again. The window size is fixed at
  compile time (`WINDOW_BINS` in `pixelhistogram_kernel.cuh`).
- The simulated updates are not a time. The kernel is not limited by GPU
  memory bandwidth (see the layout result above), so twice the updates won't
  mean twice the time. The window size has not been timed on the GPU.

### To reproduce

From `python/tests/perf/hist`. This reruns the pixel-major folder; the
bin-major folder needs commit `c7fa15e` or earlier, with the layout switch set
to bin-major and `R` pointing at `binMajor`:

```bash
R=results/A100/2026-11-11_window/pixelMajor
M="--cache-control none --kernel-name regex:fill_ --launch-skip 5 --launch-count 5 --print-summary per-kernel --metrics gpu__time_duration.avg,lts__t_sector_hit_rate.pct,dram__bytes_read.sum,dram__bytes_write.sum"
for k in window direct; do for b in 250 2000 4000; do
  ncu $M python nsys_hist_probe.py u16 20000 $b 1024 4096 --kernel $k 2>&1 | grep -E "^dtype|duration|hit_rate|dram__"
done; done
for k in window direct; do for b in 250 500 1000 2000 4000; do
  python cpu_vs_gpu.py --n-bins $b --threads 30 --batch 1024 --gpu-dtype u16 f32 f64 --kernel $k --csv $R/runs.csv
done; done
```

Divide the duration by 1024 to get µs/frame.
