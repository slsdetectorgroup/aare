# PixelHistogramCUDA nsys probes

GPU time of `PixelHistogramCUDA`, traced with Nsight Systems on real MOENCH
frames. Two costs, reported separately:

- **Fill, per frame**: the H2D copy of the frame and the fill kernel
  (`fill_window` or `fill_direct`). They overlap (two frame slots, a copy
  stream and a compute stream), so throughput is bound by the taller of the
  two, not their sum.
- **Readout, once per `values()` call**: one D2H of `rows × cols × n_bins`
  counts. With a storage type other than the device counters' (`uint32`), the
  `cap_counts` kernel runs first; the probes use `uint32`, so traces made
  after 12 Nov 2026 have no readout kernel. It doesn't depend on the number
  of frames.

These are *not* throughput numbers. nsys inflates wall clock by tracing every
CUDA call. Use them for "what does the kernel cost" and "which engine binds",
and measure sustained FPS separately without a profiler.

## Files

| file | role |
|---|---|
| `run_probes.py` | driver: runs each config under nsys, exports SQLite, writes `probes.csv`; also the sweep mode and `--reanalyze` (below) |
| `nsys_hist_probe.py` | the traced process: fill one histogram with N frames, read it out, exit |
| `hist_common.py` | histogram settings: headers for the rebuild guard, kernel names, results folder |
| `plot_fill_bound.py` | from a sweep's `probes.csv`: copy and kernel time per frame against the bin width, one panel per input type |
| `plot_batch.py` | from a batch sweep's `probes.csv`: kernel and copy time per frame against the batch size |
| `cpu_vs_gpu.py` | not nsys: wall-clock CPU vs GPU of the whole pipeline, checks that the results are identical, appends to a `runs.csv` |
| `plot_cpu_vs_gpu.py` | throughput and speed-up plots from a `cpu_vs_gpu.py` CSV |
| `window_hits.py` | numpy simulation of `fill_window`: hit rate and memory traffic per bin width and window size |
| `../gpu_span.py`, `../common.py` | shared with the other probes, see [`../README.md`](../README.md) |

## Requirements

As for the cluster-finder probes ([`../cf/README.md`](../cf/README.md)): a
fresh CUDA build of the extension, a Python of the same minor version with
numpy and matplotlib, Nsight Systems, the MOENCH dataset, an idle GPU. Also:

- **Host memory**: only `--chunk` frames are loaded (2000 by default, 640 MB as
  uint16, twice that as float32), plus the output histogram. The chunk is
  pinned once and queued repeatedly until `--frames` frames have gone through:
  the histogram doesn't depend on frame order, and every pass is a real H2D
  copy.
- **GPU memory**: the accumulator, `rows × cols × n_bins × 4` bytes (640 MB
  for 1000 bins of 400 × 400 pixels), plus two frame batches
  (`2 × batch × rows × cols × itemsize`). `values()` adds a temporary
  histogram of the storage type, except for `uint32` counts, as here.

## Run

```bash
cd python/tests/perf/hist
python run_probes.py                # full sweep: 4 configs, 20k frames each
python run_probes.py --frames 2000  # smoke test only; clocks do not ramp
python run_probes.py --only u16_b1000_bs64 --tag mychange
```

| label | frames | bins | batch | what it shows |
|---|---|---|---|---|
| `u16_b1000_bs64` | uint16 | 1000 | 64 | reference: raw frames, batch 64 (the default until Nov 2026) |
| `f32_b1000_bs64` | float32 | 1000 | 64 | twice the H2D bytes per frame |
| `u16_b100_bs64` | uint16 | 100 | 64 | 64 MB histogram, about the size of L2: increments mostly hit cache |
| `u16_b1000_bs512` | uint16 | 1000 | 512 | fewer, longer copies and kernels |

The range is taken from the data (0.1 to 99.9 percentile of the first frames),
so nearly every value is histogrammed; the probe prints the fraction in range.
Counts are uint32.

Output goes to `results/<date>[_tag]/`: `env.json`, one `.nsys-rep` +
`.sqlite` per config, and `probes.csv`. Re-running one config replaces its own
row and keeps the others. `probes.csv` and `env.json` can be committed with a
result; the traces are gitignored.

Sweep mode, instead of the 4 configs: every input type × bin count × kernel
at one batch size and a fixed range, into a folder of your choice. Plot it
with `plot_fill_bound.py`; repeat with several `--batch` values into one
folder for `plot_batch.py`.

```bash
python run_probes.py --dtypes u16 f32 f64 --bins 250 500 1000 2000 4000 \
    --batch 1024 --range 3500 7500 --kernel window direct --chunk 4096 \
    --outdir results/<gpu>/<date>/nsys
python plot_fill_bound.py results/<gpu>/<date>/nsys/probes.csv
```

`--reanalyze DIR` recomputes `DIR/probes.csv` from the `.sqlite` traces in it,
without running anything: use it after a change to the analysis.

To inspect one trace by hand:

```bash
nsys stats --report cuda_gpu_sum results/<dir>/probe_u16_b1000_bs64.nsys-rep
python ../gpu_span.py results/<dir>/probe_u16_b1000_bs64.sqlite 20000 fill_window
```

Without the kernel name, `gpu_span.py` also counts the readout kernel and the
readout D2H as if they were per-frame work.

## Reading the numbers

- The fill window starts at the first copy, and every copy is counted.
  (`../gpu_span.py`, which the command above uses, starts at the first kernel
  and so leaves the first batch's copy out: about 10 % at batch 4096 over
  40 960 frames. `run_probes.py` doesn't use that part of it.)
- A kernel below the H2D bar is invisible end to end; kernel work only pays
  once the kernel is the taller bar.
- Readout cost grows with `rows × cols × n_bins` and is paid per `values()`
  call: amortise it by reading out rarely, and pass a registered `out` array
  so the D2H runs at full pinned bandwidth.

Results, with READMEs: [`results/A100/`](results/A100/README.md) and
[`results/RTX4090/`](results/RTX4090/README.md).
