# PixelHistogramCUDA nsys probes

GPU time of `PixelHistogramCUDA`, traced with Nsight Systems on real MOENCH
frames. Two costs, reported separately:

- **Fill, per frame**: the H2D copy of the frame and the `fill_bin_major`
  kernel. They overlap (two frame slots, a copy stream and a compute stream),
  so throughput is bound by the taller of the two, not their sum.
- **Readout, once per `values()` call**: the `bin_to_pixel_major` kernel, then
  one D2H of `rows × cols × n_bins` counts. It doesn't depend on the number of
  frames.

These are *not* throughput numbers. nsys inflates wall clock by tracing every
CUDA call. Use them for "what does the kernel cost" and "which engine binds",
and measure sustained FPS separately without a profiler.

## Files

| file | role |
|---|---|
| `run_probes.py` | driver: runs each config under nsys, exports SQLite, writes `probes.csv` |
| `nsys_hist_probe.py` | the traced process: fill one histogram with N frames, read it out, exit |
| `hist_common.py` | histogram settings: headers for the rebuild guard, kernel names, results folder |
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
- **GPU memory**: about `2 × rows × cols × n_bins × 4` bytes, plus two frame
  batches. The accumulator stays allocated and `values()` allocates a
  temporary of the same size for the transpose. 1000 bins of 400 × 400 pixels
  needs about 1.4 GB.

## Run

```bash
cd python/tests/perf/hist
python run_probes.py                # full sweep: 4 configs, 20k frames each
python run_probes.py --frames 2000  # smoke test only; clocks do not ramp
python run_probes.py --only u16_b1000_bs64 --tag mychange
```

| label | frames | bins | batch | what it shows |
|---|---|---|---|---|
| `u16_b1000_bs64` | uint16 | 1000 | 64 | reference: raw frames, default batch size |
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

To inspect one trace by hand:

```bash
nsys stats --report cuda_gpu_sum results/<dir>/probe_u16_b1000_bs64.nsys-rep
python ../gpu_span.py results/<dir>/probe_u16_b1000_bs64.sqlite 20000 fill_bin_major
```

Without the kernel name, `gpu_span.py` also counts the readout kernel and the
readout D2H as if they were per-frame work.

## Reading the numbers

- The fill window starts at the first fill kernel, so the copy of the very
  first batch is left out: one batch in `frames / batch`, under 0.5 % at the
  defaults.
- A kernel below the H2D bar is invisible end to end; kernel work only pays
  once the kernel is the taller bar.
- Readout cost grows with `rows × cols × n_bins` and is paid per `values()`
  call: amortise it by reading out rarely, and pass a registered `out` array
  so the D2H runs at full pinned bandwidth.

No reference results yet: the first sweep on an idle GPU will be the
baseline.
