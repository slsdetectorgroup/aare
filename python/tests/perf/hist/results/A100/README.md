# A100 probe results (Merlin7)

NVIDIA A100-SXM4-80GB (40 MB L2) on Merlin7 (gmerlin7, a100 partitions), CUDA
12.9.1, nsys 2025.1.3 with CUDA event trace off. Raw MÖNCH frames (Cu
fluorescence, 400 × 400, uint16, no pedestal subtracted); the range comes from
the data, [4171, 5744) ADU. Driver and git state are in each run's `env.json`.

| folder | layout | frames | configs |
|---|---|---|---|
| `2026-10-29_bin-major` | bin-major | 20 000 | the 4 configs of `run_probes.py` |

To be PCIe-bound, the fill kernel must take less time than the H2D copy:
12 µs/frame for uint16 frames, 24 µs for float32.

## Probe baseline (`2026-10-29_bin-major`)

Fill in µs/frame, readout in ms per `values()` call.

| config | fill kernel | H2D | bound | FPS | readout kernel | readout D2H |
|---|--:|--:|---|--:|--:|--:|
| `u16_b1000_bs64` | 23.3 | 12.0 | kernel | 42 900 | 5.8 | 24.4 |
| `f32_b1000_bs64` | 23.5 | 24.0 | H2D ≈ kernel | 41 700 | 5.8 | 24.4 |
| `u16_b100_bs64` | 4.4 | 12.0 | H2D | 83 100 | 0.4 | 2.4 |
| `u16_b1000_bs512` | 22.9 | 11.7 | kernel | 43 600 | 5.8 | 24.4 |

At 1000 bins the fill kernel binds, so uint16 input gains nothing yet, and
batch size changes less than 2 %.

## Fill kernel vs binning and layout (ncu, not stored as files)

`fill_bin_major` under ncu, one launch = 64 frames, in µs/frame with the L2
sector hit rate. "Centred" frames have each pixel's median subtracted and stay
uint16 (`--center 1000`, range [937, 2127)), so neighbouring pixels share bins.
Pixel-major is a build with `detail::PIXEL_MAJOR = true`.

| bins | raw width (ADU) | raw, bin-major | raw, pixel-major | centred width (ADU) | centred, bin-major | centred, pixel-major |
|--:|--:|--:|--:|--:|--:|--:|
| 100 | 15.7 | **4.5** (88 %) | 7.5 (89 %) | 11.9 | **2.6** (85 %) | 7.6 (89 %) |
| 200 | 7.9 | 9.4 (77 %) | **8.9** (86 %) | 6.0 | **3.6** (86 %) | 9.2 (86 %) |
| 500 | 3.1 | 19.1 (65 %) | **12.0** (80 %) | 2.4 | **6.0** (83 %) | 13.7 (77 %) |
| 1000 | 1.6 | 23.1 (62 %) | **16.1** (73 %) | 1.2 | **11.9** (71 %) | 18.8 (68 %) |
| 2000 | 0.8 | 26.6 (60 %) | **21.2** (65 %) | 0.6 | **15.0** (68 %) | 22.3 (64 %) |

1000 bins over a 10× wider range (`--range-scale 10`: 15.7 ADU bins, 640 MB
accumulator) gives 4.5 µs/frame and 88 %, the same as 100 bins over the normal
range (64 MB accumulator).

- The kernel's cost follows how many distinct (bin, pixel) cells the frames
  hit, which the bin width sets. The accumulator size doesn't matter.
- Raw frames with bin-major are DRAM-bound from 200 bins up, at ~620 GB/s;
  from 1000 bins, nearly every increment is a 64 B DRAM fetch.
- Bin-major is 2–4× faster when neighbours share bins (centred frames).
  Pixel-major helps raw frames from 200 bins up, but stays above ~7.5 µs/frame
  because every warp increment touches 32 sectors.
- Under 12 µs/frame (uint16): raw bin-major up to ~280 bins, raw pixel-major
  up to ~500, centred bin-major up to ~1000. No layout keeps raw uint16 frames
  PCIe-bound at 1000 bins.

To reproduce, from `python/tests/perf/hist`:

```bash
M="--cache-control none --kernel-name fill_bin_major --launch-skip 20 --launch-count 5 --print-summary per-kernel --metrics gpu__time_duration.avg,lts__t_sector_hit_rate.pct,dram__bytes_read.sum,dram__bytes_write.sum"
for b in 100 200 500 1000 2000; do
  ncu $M python nsys_hist_probe.py u16 2000 $b 64 2000 2>&1 | grep -E "^dtype|duration|hit_rate|dram__"
done
# add --center 1000 for centred frames, or use --range-scale 10 at 1000 bins
```

Divide the duration by 64 to get µs/frame.
