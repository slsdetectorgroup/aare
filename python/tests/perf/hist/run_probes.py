#!/usr/bin/env python3
"""nsys probe sweep for PixelHistogramCUDA: fill cost per frame, readout cost.

    python3.11 run_probes.py                       # full sweep, 20k frames
    python3.11 run_probes.py --frames 2000         # quick check (clocks may not ramp)
    python3.11 run_probes.py --only u16_b1000_bs64 --tag mytag

Per config: an .nsys-rep + .sqlite in results/<date>[_tag]/ and one row in
probes.csv (re-running a config replaces its own row only).

Two separate costs:
  fill     per frame: H2D of the frame and fill_bin_major, overlapping. The
           roofline is the taller of the two.
  readout  once per values() call: bin_to_pixel_major, then one D2H of
           rows * cols * n_bins counts. Independent of the number of frames.
"""
from __future__ import annotations

import argparse
import csv
import sqlite3
import subprocess
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import hist_common  # noqa: E402  (puts ../ and the build dir on sys.path)
from hist_common import common  # noqa: E402
import gpu_span  # noqa: E402

HERE = Path(__file__).resolve().parent

# (dtype, n_bins, batch, label)
#   u16_b1000_bs64   reference: raw frames, default batch size
#   f32_b1000_bs64   twice the H2D bytes per frame
#   u16_b100_bs64    histogram of 160k pixels x 100 bins x 4 B = 64 MB, about
#                    the size of L2: the fill kernel's scattered increments
#                    mostly hit cache
#   u16_b1000_bs512  larger batches: fewer, longer copies and kernels
CONFIGS = [
    ("u16", 1000, 64, "u16_b1000_bs64"),
    ("f32", 1000, 64, "f32_b1000_bs64"),
    ("u16", 100, 64, "u16_b100_bs64"),
    ("u16", 1000, 512, "u16_b1000_bs512"),
]


def readout(sqlite_path) -> dict:
    """Total time of the readout kernel and of all D2H copies."""
    db = sqlite3.connect(str(sqlite_path))
    k = [e - s for s, e, name in db.execute(gpu_span.KERNELS)
         if hist_common.READOUT_KERNEL in name]
    d2h_ns, d2h_bytes = db.execute(
        "SELECT COALESCE(SUM(end-start), 0), COALESCE(SUM(bytes), 0) "
        "FROM CUPTI_ACTIVITY_KIND_MEMCPY WHERE copyKind=2").fetchone()
    return {"readout_kernel_n": len(k), "readout_kernel_ms": sum(k) / 1e6,
            "readout_D2H_ms": d2h_ns / 1e6, "readout_D2H_MB": d2h_bytes / 1e6,
            "readout_D2H_GBps": d2h_bytes / d2h_ns if d2h_ns else 0.0}


def run_one(dtype, n_bins, batch, label, n_frames, chunk, outdir) -> dict | None:
    rep = outdir / f"probe_{label}"
    print(f"\n--- {label}: {dtype} frames, {n_bins} bins, batch={batch}, N={n_frames} ---")

    prof = [common.NSYS, "profile", *common.nsys_profile_flags(),
            "-o", str(rep),
            sys.executable, str(HERE / "nsys_hist_probe.py"),
            dtype, str(n_frames), str(n_bins), str(batch), str(chunk)]
    p = subprocess.run(prof, capture_output=True, text=True)
    for line in p.stdout.splitlines():
        if line.strip().startswith(("dtype=", "H2D/frame", "wall")):
            print("   ", line.strip())
    # nsys writes a report even when the traced process crashes, and returns
    # the child's exit code: check it, or a dead probe surfaces later as a
    # missing CUPTI table in the export.
    if p.returncode != 0 or not rep.with_suffix(".nsys-rep").exists():
        print(f"    FAILED (exit {p.returncode}): {p.stderr.strip()[-400:]}")
        return None

    subprocess.run([common.NSYS, "stats", "--force-export=true", "--report",
                    "cuda_gpu_sum", str(rep.with_suffix(".nsys-rep"))],
                   capture_output=True, text=True)
    sq = rep.with_suffix(".sqlite")
    if not sq.exists():
        print("    FAILED: no sqlite export")
        return None

    r = gpu_span.analyze(sq, n_frames, hist_common.FILL_KERNEL,
                         engines=("kernel", "H2D"))
    r.update(readout(sq))
    r.update(label=label, dtype=dtype, n_bins=n_bins, batch=batch)
    print(f"    fill: kernel {r['kernel_us_per_frame']:5.2f} us (duty {r['kernel_duty_pct']:4.1f}%)  "
          f"H2D {r['H2D_us_per_frame']:5.2f} ({r['H2D_duty_pct']:4.1f}%)  "
          f"-> {r['bottleneck']}-bound at {r['roofline_us_per_frame']:.2f} us/frame "
          f"= {r['roofline_fps']:,.0f} FPS")
    print(f"    readout: kernel {r['readout_kernel_ms']:.2f} ms  D2H {r['readout_D2H_ms']:.2f} ms "
          f"({r['readout_D2H_MB']:,.0f} MB at {r['readout_D2H_GBps']:.1f} GB/s)")
    return r


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--frames", type=int, default=20_000)
    ap.add_argument("--chunk", type=int, default=2000,
                    help="frames held in host memory and re-queued until --frames")
    ap.add_argument("--tag", default="")
    ap.add_argument("--only", nargs="+", default=None, help="subset of labels")
    args = ap.parse_args()

    common.assert_build_fresh(hist_common.HEADERS)
    common.assert_idle_gpu()
    env = common.capture_env()
    outdir = hist_common.results_dir(args.tag)
    common.write_env(outdir / "env.json", env)
    print(f"build: git={env['git_rev']}{' (dirty)' if env['git_dirty'] else ''}")
    print(f"out:   {outdir}")

    rows = []
    for dtype, n_bins, batch, label in CONFIGS:
        if args.only and label not in args.only:
            continue
        r = run_one(dtype, n_bins, batch, label, args.frames, args.chunk, outdir)
        if r:
            rows.append(r)

    if not rows:
        return 1
    out = outdir / "probes.csv"
    prior = list(csv.DictReader(out.open())) if out.exists() else []
    fresh = {r["label"] for r in rows}
    merged = [r for r in prior if r["label"] not in fresh] + rows
    with out.open("w", newline="") as fh:
        w = csv.DictWriter(fh, fieldnames=list(rows[0]))
        w.writeheader()
        w.writerows(merged)

    print("\n=== fill (us/frame) and readout (ms, once per values()) ===")
    print(f"{'config':<18} {'kernel':>8} {'H2D':>8}  {'bound':<6} {'FPS':>10}  "
          f"{'rd kernel':>9} {'rd D2H':>8}")
    for r in rows:
        print(f"{r['label']:<18} {r['kernel_us_per_frame']:8.2f} {r['H2D_us_per_frame']:8.2f}  "
              f"{r['bottleneck']:<6} {r['roofline_fps']:10,.0f}  "
              f"{r['readout_kernel_ms']:9.2f} {r['readout_D2H_ms']:8.2f}")
    print(f"\n-> {out}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
