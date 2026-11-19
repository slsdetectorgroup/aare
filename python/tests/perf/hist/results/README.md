# Performance results of the CUDA pixel histograms

Measurements of `PixelHistogramCUDA` and `PedestalTrackingPixelHistogramCUDA`.
The scripts are in `python/tests/perf/hist`.

| Folder | What is in it |
|---|---|
| `A100/` | NVIDIA A100 on Merlin7: nsys, ncu and `cpu_vs_gpu.py` runs; see its README |
| `RTX4090/` | RTX 4090 in pc-moench-04: nsys runs; see its README |
| `window_hits_2026-11-10/` | `window_hits.py`, a numpy simulation of the window kernel's GPU memory updates; no GPU timing |

Each dated folder has its `probes.csv` (or `runs.csv`), the `env.json` of the
machine, build and git state, and the plots. The `.nsys-rep` and `.sqlite`
traces are not in git.

## Terms

- **Fill**: everything `fill_histogram()` does. It copies a batch of frames
  from host memory to the GPU and runs the fill kernel on it. The copy of the
  next batch runs while the kernel fills the current one, so the fill runs at
  the speed of the slower of the two.
- **Copy limit**: the time to copy one frame over PCIe, about 26 GB/s on both
  GPUs: 12 µs per frame for uint16 frames of 400 × 400 pixels, 24 µs for
  float32, 48 µs for float64. A fill at this limit is **copy-bound**: a faster
  kernel no longer helps.
- **Readout (`values()`)**: reads the histogram back to host memory, once,
  after the fill. It is timed separately (`readout_*` columns) and is not part
  of any fill time below.
- **Accumulator layout**: how the histogram is stored in GPU memory.
  **Bin-major**: `[bin][pixel]`. **Pixel-major**: `[pixel][bin]`, the order
  `values()` returns. Pixel-major is the only layout in the code since 11 Nov;
  the bin-major runs used a compile-time switch, last present in commit
  `c7fa15e`.
- **Direct kernel** (`fill_direct`): every value increments its bin directly
  in GPU memory.
- **Window kernel** (`fill_window`, the default): each pixel counts 128
  consecutive bins around its noise peak (the "window") in fast on-chip shared
  memory during a batch. Values outside the window, mostly photons, go
  straight to GPU memory. At the end of the batch the window counts are added
  to the histogram in GPU memory (the "flush"). The window places itself on
  the first batch and follows the pixel's pedestal afterwards.
- **Pedestal tracking** (`PedestalTrackingPixelHistogramCUDA`): bins the
  residual, raw value minus the pixel's pedestal, and updates the pedestal
  with the values whose residual is within `n_sigma` × sigma.
- **ncu** times one kernel alone. **nsys** records every kernel and copy of a
  whole run (the times below). `cpu_vs_gpu.py` times the fill and the readout
  from Python.

## How the nsys times are computed

`run_probes.py` runs `nsys_hist_probe.py` under nsys. The probe fills one
histogram with N frames (20 000, or 40 960 in the batch-size sweeps) and
reads it out. nsys records when each frame copy (host to GPU) and each fill
kernel launch starts and ends:

```text
copy stream:     [copy 1][copy 2][copy 3] ... [copy M]
compute stream:          [fill 1][fill 2] ... [fill M-1][fill M]
                 ↑                                              ↑
                 start of the first copy        end of the last kernel
                 |←───────────── fill time × N ────────────────→|
```

Batch k+1 is copied while batch k is filled, so the copy and the kernel
overlap, and the slower of the two sets the fill time.

| Quantity, µs per frame | Formula | CSV column |
|---|---|---|
| **Kernel busy time** | (sum of the durations of all fill kernel launches) / N | `kernel_us_per_frame` |
| **Copy busy time** | (sum of the durations of all frame copies to the GPU) / N | `H2D_us_per_frame` |
| **Fill time** | (end of the last kernel − start of the first copy) / N | `window_us_per_frame` |

- Busy times leave out the gaps between launches. Times when two launches
  overlap are counted once; fill kernels run on one stream and never overlap,
  so the busy time is just the sum.
- The fill time is about the larger of the two busy times. On top of that come
  the first copy and the last kernel, which overlap nothing; they weigh more
  at large batch sizes.
- `bottleneck` names the larger busy time, `roofline_us_per_frame` is its
  value, and `roofline_fps` = 10⁶ / `roofline_us_per_frame`.
- Only copies on the frame stream count as frame copies.
  `PedestalTrackingPixelHistogramCUDA` uploads its seed pedestal with a few
  small copies before the fill; they are left out.

### The plots

- **`fill_bound.png`** (`plot_fill_bound.py`): busy time per frame against the
  bin width, batch 1024, one panel per input type. The black dashed line is
  the copy; the coloured lines are the kernels (dotted, open markers: pedestal
  tracking). A kernel line under the copy line is hidden behind the copy.
- **`batch_size.png`** (`plot_batch.py`): against the batch size, window
  kernel. Left: the same busy times as `fill_bound.png`, one line per bin
  width. Right: the fill time, what the pipeline actually delivers; it lies on
  the copy line when the fill is copy-bound.

Both plots repeat these definitions in a footnote, so they can be quoted on
their own.
