"""Simulate the planned shared-memory window of the fill kernel, in numpy.

    window_hits.py [--n-frames N] [--batch S] [--xmin X] [--xmax X]
                   [--adu-per-bin D ...] [--window W ...] [--pixel-stride K]
                   [--csv PATH]

Answers one question before any CUDA is written: on real frames, what
fraction of the values would the planned kernel count in shared memory, and
how much global-memory traffic would be left?

The kernel being simulated (spec: the PixelHistogramCUDA guide, section
"Next: shared-memory window"). Per pixel, a window of W consecutive bins
starting at s = int(centre) - W/2, clamped to [0, nbins - W]:

  step 1  centre not placed yet (first batch, or flagged by step 3):
          mean bin of the batch's in-range values, then the mean again over
          the values within W/2 bins of it (drops the photons)
  bin     in-window values -> shared counter, all others -> global ++
  flush   every non-zero window counter -> one global +=
  step 2  centre <- mean bin of the window counts
  step 3  window caught under half of the in-range values -> step 1 again

Printed per (bin width, W):

  hit b1     fraction of in-range values counted in the window, first batch
  hit rest   the same over all later batches
  hit min    the worst later batch
  miss/val   values that miss the window, per in-range value: each is a
             random global read-modify-write, as every value is today (1.00)
  flush/val  flush writes per in-range value: at most W per pixel per batch,
             contiguous per pixel in pixel-major, so cheaper than a miss.
             Falls as the batch grows.
  refire %   pixel-batches sent back to step 1, after the first batch
  move p95   95th percentile over pixels of |final centre - centre after the
             first batch|, in ADU: pedestal drift, plus the pull of the
             high-side tail on the window mean when W is wide

Needs only numpy and the aare File reader (no GPU). 20 000 frames of
400 x 400 use 6.4 GB of host memory; --pixel-stride K keeps every K-th pixel.
"""
import argparse
import csv
import sys
import time
from pathlib import Path

import numpy as np

sys.path.insert(0, str(Path(__file__).resolve().parent))
import hist_common  # noqa: E402,F401  (puts ../ and the build dir on sys.path)
from hist_common import common  # noqa: E402

from aare import File  # noqa: E402

CSV_COLUMNS = ["adu_per_bin", "n_bins", "window", "smem_kb", "hit_b1",
               "hit_rest", "hit_min", "miss_per_value", "flush_per_value",
               "refire_pct", "move_p95_adu", "n_frames", "batch", "pixels"]
BLOCK_SIZE = 128  # PixelHistogramCUDA default, for the shared-memory column

ap = argparse.ArgumentParser()
ap.add_argument("--n-frames", type=int, default=20_000)
ap.add_argument("--batch", type=int, default=128)
ap.add_argument("--xmin", type=float, default=3500)
ap.add_argument("--xmax", type=float, default=7500)
ap.add_argument("--adu-per-bin", type=float, nargs="+", default=[16, 8, 4, 2, 1])
ap.add_argument("--window", type=int, nargs="+", default=[32, 64, 128])
ap.add_argument("--pixel-stride", type=int, default=1,
                help="keep every K-th pixel (faster, for a first look)")
ap.add_argument("--csv", type=Path, help="write one row per (bin width, W)")
args = ap.parse_args()

# ---- frames ----------------------------------------------------------------
t0 = time.perf_counter()
frames = File(common.DATA_FILE).read_n(args.n_frames)
n_frames = frames.shape[0]
frames = frames.reshape(n_frames, -1)[:, ::args.pixel_stride]  # (frames, pixels)
npix = frames.shape[1]
print(f"read {n_frames} frames, {npix} pixels (stride {args.pixel_stride}) "
      f"in {time.perf_counter() - t0:.1f} s")

# ---- noise and photons, from the first 1000 frames (context only) ------------
head = frames[:1000].astype(np.float32)
med = np.median(head, axis=0)
sigma = 1.4826 * np.median(np.abs(head - med), axis=0)  # robust: photons barely move it
photon_like = (head > med + 5 * np.maximum(sigma, 1)).mean()
in_range = ((frames >= args.xmin) & (frames < args.xmax)).mean()
print(f"noise sigma (ADU): median {np.median(sigma):.1f}, "
      f"p95 {np.percentile(sigma, 95):.1f}, max {sigma.max():.1f}")
print(f"values > median + 5 sigma (photon-like): {photon_like:.2%}; "
      f"in [{args.xmin:g}, {args.xmax:g}): {in_range:.2%}")
del head


# ---- the simulated kernel ----------------------------------------------------
def bins_of(batch, xmin, xmax, scale, nbins):
    """Bin index as the kernel computes it (float axis), -1 if out of range."""
    v = batch.astype(np.float32)
    inr = (v >= xmin) & (v < xmax)
    b = np.minimum(((v - xmin) * scale).astype(np.int32), nbins - 1)
    return np.where(inr, b, -1)


def place(b, inr, n_in, W, nbins):
    """Step 1 for every pixel: trimmed mean bin of this batch's values."""
    m = np.where(n_in > 0, np.where(inr, b, 0).sum(0) / np.maximum(n_in, 1), nbins / 2)
    near = inr & (np.abs(b - m) < W / 2)
    n_near = near.sum(0)
    return np.where(n_near > 0, np.where(near, b, 0).sum(0) / np.maximum(n_near, 1), m)


results = []
for adu in args.adu_per_bin:
    nbins = int(round((args.xmax - args.xmin) / adu))
    xmin, xmax = np.float32(args.xmin), np.float32(args.xmax)
    scale = np.float32(nbins) / (xmax - xmin)
    state = {W: dict(centre=np.full(npix, np.nan), hits=[], n_in=[], flush=0,
                     n_all=0, refire=0, first=None) for W in args.window}
    t0 = time.perf_counter()

    for i, f0 in enumerate(range(0, n_frames, args.batch)):
        b = bins_of(frames[f0:f0 + args.batch], xmin, xmax, scale, nbins)
        inr = b >= 0
        n_in = inr.sum(0)

        for W, st in state.items():
            c = st["centre"]
            unset = np.isnan(c)
            if unset.any():                                   # step 1
                c = np.where(unset, place(b, inr, n_in, W, nbins), c)
            s = np.clip(np.floor(c).astype(np.int64) - W // 2, 0, max(nbins - W, 0))

            k = b - s                                         # bin
            hit = inr & (k >= 0) & (k < W)
            hits = hit.sum(0)
            wsum = np.where(hit, k, 0).sum(0)
            # flush: one global += per non-zero window counter
            cols = np.nonzero(hit)[1]
            occupied = np.bincount(cols * W + k[hit], minlength=npix * W) > 0
            flush = occupied.sum()

            refire = (n_in > 0) & (2 * hits < n_in)           # steps 2 and 3
            new = np.where(hits > 0, s + wsum / np.maximum(hits, 1), c)
            new = np.where(n_in == 0, c, np.where(refire, np.nan, new))
            st["centre"] = new
            if i == 0:
                st["first"] = new.copy()
            else:
                st["refire"] += refire.sum()

            st["hits"].append(hits.sum())
            st["n_in"].append(n_in.sum())
            st["flush"] += flush
            st["n_all"] += n_in.sum()

    n_batches = i + 1
    for W, st in state.items():
        hits, n_in = np.array(st["hits"]), np.array(st["n_in"])
        rest = slice(1, None)
        hit_rest = hits[rest].sum() / max(n_in[rest].sum(), 1)
        per_batch = hits[rest] / np.maximum(n_in[rest], 1)
        a, z = st["first"], st["centre"]
        ok = np.isfinite(a) & np.isfinite(z)
        move = np.percentile(np.abs(z[ok] - a[ok]) * adu, 95) if ok.any() else np.nan
        results.append(dict(
            adu_per_bin=f"{adu:g}", n_bins=nbins, window=W,
            smem_kb=f"{W * BLOCK_SIZE * 2 / 1024:g}",
            hit_b1=f"{hits[0] / max(n_in[0], 1):.4f}",
            hit_rest=f"{hit_rest:.4f}",
            hit_min=f"{per_batch.min() if per_batch.size else np.nan:.4f}",
            miss_per_value=f"{1 - hits.sum() / max(st['n_all'], 1):.4f}",
            flush_per_value=f"{st['flush'] / max(st['n_all'], 1):.4f}",
            refire_pct=f"{100 * st['refire'] / max(npix * (n_batches - 1), 1):.3f}",
            move_p95_adu=f"{move:.2f}",
            n_frames=n_frames, batch=args.batch, pixels=npix))
    print(f"{adu:g} ADU per bin ({nbins} bins): {time.perf_counter() - t0:.0f} s")

# ---- report --------------------------------------------------------------------
print(f"\n{n_frames} frames, batch {args.batch}, range [{args.xmin:g}, {args.xmax:g}). "
      f"Today's kernel: miss/val 1.00, flush/val 0.")
hdr = (f"{'ADU/bin':>7} {'bins':>5} {'W':>4} {'smem KB':>7} {'hit b1':>7} "
       f"{'hit rest':>8} {'hit min':>7} {'miss/val':>8} {'flush/val':>9} {'refire %':>8} "
       f"{'move p95':>8}")
print(hdr)
print("-" * len(hdr))
for r in results:
    print(f"{r['adu_per_bin']:>7} {r['n_bins']:>5} {r['window']:>4} {r['smem_kb']:>7} "
          f"{r['hit_b1']:>7} {r['hit_rest']:>8} {r['hit_min']:>7} "
          f"{r['miss_per_value']:>8} {r['flush_per_value']:>9} {r['refire_pct']:>8} "
          f"{r['move_p95_adu']:>8}")

if args.csv:
    args.csv.parent.mkdir(parents=True, exist_ok=True)
    with args.csv.open("w", newline="") as f:
        w = csv.DictWriter(f, fieldnames=CSV_COLUMNS, lineterminator="\n")
        w.writeheader()
        w.writerows(results)
    print(f"wrote {args.csv}")
