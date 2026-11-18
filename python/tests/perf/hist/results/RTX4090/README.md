# RTX 4090 results for PixelHistogramCUDA (pc-moench-04)

NVIDIA GeForce RTX 4090 (72 MB L2 cache) in pc-moench-04, driver 595.71.05,
CUDA 12.4, nsys 2024.5.1. The card is shared with other jobs: these runs were
made while it was idle (checked by `run_probes.py`). Same data as the A100
runs: raw MÖNCH frames, 400 × 400 pixels, uint16. Driver and git state in each
folder's `env.json`; terms (fill, copy limit, layouts, window and direct
kernels) are explained in `../A100/README.md`.

| Folder | What was run |
|---|---|
| `2026-11-11_window/pixelMajor/nsys` | copy or kernel: uint16 / float32 / float64, 250–4000 bins, window and direct kernels, batch 1024 |
| `2026-11-11_window/pixelMajor/nsys_batch` | batch size 64–4096: uint16, window kernel, 1000 / 2000 / 4000 bins |
| `2026-11-18_tracking/nsys` | `PedestalTrackingPixelHistogramCUDA` (`n_sigma` 3) next to `PixelHistogramCUDA`: uint16, 250–4000 bins, window and direct kernels, batch 1024 |
| `2026-11-18_tracking/nsys_nsigma` | pedestal tracking, `n_sigma` 0 / 1 / 3 / 5: 1 ADU per bin, window kernel, batch 1024 |
| `2026-11-18_tracking/nsys_batch` | pedestal tracking, batch size 64–4096: window kernel, 1000 / 2000 / 4000 bins |

Pixel-major layout, range [3500, 7500) ADU. The `.nsys-rep` and `.sqlite`
traces are not in git.

## Copy or kernel? (`nsys/`, plot `fill_bound.png`)

**Busy time per frame (µs), 16 → 1 ADU per bin, next to the A100**

| Input frames | Copy in, RTX 4090 | Copy in, A100 | Window kernel, RTX 4090 | Window kernel, A100 | Direct kernel, RTX 4090 | Direct kernel, A100 |
|---|--:|--:|--:|--:|--:|--:|
| uint16 | 12.1–12.7 | 12.0 | 1.9 → 3.8 | 2.9 → 5.6 | 5.5 → **13.4** | 7.7 → **20.1** |
| float32 | 24.2–25.3 | 24.0 | 1.9 → 3.8 | 2.8 → 5.4 | 5.4 → 13.6 | 7.6 → 20.2 |
| float64 | 48.8–50.8 | 48.0 | 2.5 → 4.3 | 2.9 → 5.5 | 5.3 → 14.5 | 7.8 → 20.6 |

- With the window kernel the copy limits the fill everywhere, as on the
  A100: the kernel takes at most a third of the uint16 copy time.
- The direct kernel limits the uint16 fill only at 1 ADU per bin on the
  RTX 4090 (from 2 ADU on the A100).
- Both kernels run about 1.5× faster than on the A100 (larger L2 cache,
  higher clocks). The copy is about as fast, but varies by up to 5 % from
  run to run, where the A100's is the same to 0.1 µs.

## Batch size (`nsys_batch/`, plot `batch_size.png`)

**Window kernel busy time per frame (µs), uint16 frames**

| Batch size | 4 ADU per bin | 2 ADU per bin | 1 ADU per bin | 1 ADU per bin, A100 |
|--:|--:|--:|--:|--:|
| 64 | 5.5 | 7.9 | **11.4** | **11.9** |
| 128 | 4.3 | 5.6 | 7.8 | 9.1 |
| 256 | 3.4 | 4.2 | 5.6 | 7.1 |
| 512 | 2.9 | 3.5 | 4.4 | 6.0 |
| 1024 | 2.6 | 3.1 | 3.7 | 5.3 |
| 4096 | 2.2 | 2.7 | 3.3 | 4.6 |

The copy takes 12.2–12.7 µs/frame at every batch size, and the fill stays at
the copy time. As on the A100, batch 64 leaves almost no margin at 1 ADU per
bin, and 1024 keeps the kernel well under the copy.

The CSVs were recomputed with `run_probes.py --reanalyze`, which counts the
first batch's copy (see `../A100/README.md`).

## Pedestal tracking (`2026-11-18_tracking/`)

`PedestalTrackingPixelHistogramCUDA` on the same frames: seeded with the 1000
frames of `Cu_factor_10_pedestal`, `update_mean()`, then the fill histograms
residuals in [−1000, 3000) ADU (the raw range [3500, 7500) shifted by the
median pedestal) and updates each pixel's pedestal from the values within
`n_sigma` × sigma. The `PixelHistogramCUDA` rows were rerun the same day as a
baseline. The library is commit `9a2b474`; `env.json` says dirty because the
probe scripts were being extended.

**Kernel busy time per frame (µs), batch 1024, plot `nsys/fill_bound.png`**

| Kernel | 16 ADU per bin | 8 | 4 | 2 | 1 |
|---|--:|--:|--:|--:|--:|
| Window | 1.89 | 2.23 | 2.63 | 3.16 | 3.77 |
| Window, pedestal tracking | 4.07 | 4.30 | 4.48 | 4.84 | 4.60 |
| Direct | 5.38 | 5.68 | 5.79 | 7.29 | **13.43** |
| Direct, pedestal tracking | 5.60 | 5.58 | 5.99 | 7.82 | **13.71** |
| Copy in | 11.96 | 11.96 | 11.97 | 11.96 | 11.96 |

- The window kernel with pedestal tracking takes 4.1–4.8 µs per frame at
  every bin width, a third of the copy: the fill stays copy-bound, at
  83 600 frames per second.
- Tracking costs 0.8 µs per frame at 1 ADU per bin and 2.2 µs at 16. The
  direct kernel was already slower, and tracking adds at most 0.5 µs to it.

**Cost of the pedestal update: window kernel, 1 ADU per bin (`nsys_nsigma/`)**

| | No tracking class | `n_sigma` 0 | 1 | 3 | 5 |
|---|--:|--:|--:|--:|--:|
| Kernel (µs per frame) | 3.77 | 4.00 | 4.79 | 4.64 | 4.61 |

With `n_sigma` 0 nothing is updated: loading and storing the pedestal and
subtracting it cost 0.2 µs per frame. The update in double precision adds
0.6–0.8 µs, about the same for any `n_sigma`: a warp runs it as soon as one
of its 32 pixels passes the gate, which almost always happens. The RTX 4090
does double precision at 1/64 of its single-precision rate, and it still
doesn't matter here. A single-precision pedestal would gain nothing.

**Window kernel with pedestal tracking against the batch size (µs per frame), plot `nsys_batch/batch_size.png`**

| Batch size | 4 ADU per bin | 2 | 1 | 1, no tracking (11 Nov) |
|--:|--:|--:|--:|--:|
| 64 | 8.34 | 11.69 | **13.31** | 11.4 |
| 128 | 6.57 | 8.47 | 8.87 | 7.8 |
| 256 | 5.35 | 6.40 | 6.32 | 5.6 |
| 512 | 4.64 | 5.20 | 5.05 | 4.4 |
| 1024 | 4.27 | 4.65 | 4.52 | 3.7 |
| 4096 | 4.11 | 4.19 | 4.19 | 3.3 |

The copy takes 11.96–11.98 µs per frame. Only batch 64 at 1 ADU per bin is
kernel-bound (75 000 frames per second). From batch 128 the fill runs at the
copy's speed; the default 1024 leaves the kernel under 40 % of the copy.

## To reproduce

From `python/tests/perf/hist`:

```bash
R=results/RTX4090/2026-11-11_window/pixelMajor
python run_probes.py --dtypes u16 f32 f64 --bins 250 500 1000 2000 4000 --batch 1024 \
    --range 3500 7500 --kernel window direct --chunk 4096 --outdir $R/nsys
python plot_fill_bound.py $R/nsys/probes.csv
for bs in 64 128 256 512 1024 4096; do
  python run_probes.py --dtypes u16 --bins 1000 2000 4000 --batch $bs --range 3500 7500 \
      --kernel window --frames 40960 --chunk 4096 --outdir $R/nsys_batch
done
python plot_batch.py $R/nsys_batch/probes.csv

# pedestal tracking, 18 Nov
R=results/RTX4090/2026-11-18_tracking
python run_probes.py --dtypes u16 --bins 250 500 1000 2000 4000 --batch 1024 \
    --range 3500 7500 --kernel window direct --chunk 4096 --outdir $R/nsys
python run_probes.py --tracking --n-sigma 3 --dtypes u16 --bins 250 500 1000 2000 4000 \
    --batch 1024 --range -1000 3000 --kernel window direct --chunk 4096 --outdir $R/nsys
python plot_fill_bound.py $R/nsys/probes.csv
python run_probes.py --tracking --n-sigma 0 1 3 5 --dtypes u16 --bins 4000 --batch 1024 \
    --range -1000 3000 --kernel window --chunk 4096 --outdir $R/nsys_nsigma
for bs in 64 128 256 512 1024 4096; do
  python run_probes.py --tracking --n-sigma 3 --dtypes u16 --bins 1000 2000 4000 --batch $bs \
      --range -1000 3000 --kernel window --frames 40960 --chunk 4096 --outdir $R/nsys_batch
done
python plot_batch.py $R/nsys_batch/probes.csv
```
