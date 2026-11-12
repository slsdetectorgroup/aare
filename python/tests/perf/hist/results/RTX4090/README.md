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
```
