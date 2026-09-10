#!/usr/bin/env python3
"""nsys probe sweep: per-engine GPU time per frame, duty cycles, roofline.

    python3.11 run_probes.py                       # full sweep, 20k frames
    python3.11 run_probes.py --frames 2000         # quick check (clocks may not ramp)
    python3.11 run_probes.py --only 9x9_s1_uncontended --tag mytag

Per config: an .nsys-rep + .sqlite in results/<date>_<f32|f64>[_tag]/ and one
row in probes.csv (re-running a config replaces its own row only).

20 000 frames, not fewer: over a short run the GPU clocks never fully ramp and
the kernel reads ~10 % slow.
"""
from __future__ import annotations

import argparse
import csv
import subprocess
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import common  # noqa: E402
import gpu_span  # noqa: E402

HERE = Path(__file__).resolve().parent

# (cluster_dim, cap, n_streams, label)
#   s4 = the production configuration.
#   s1 = uncontended control: one stream, so H2D and D2H never coexist and the
#        kernel number is the kernel alone. Quote kernel changes from s1.
# The cap sets the D2H bar (the slot is copied whole regardless of fill), so a
# probe at a different cap is a different measurement.
CONFIGS = [
    (3, 3000, 4, "3x3_s4"),
    (3, 3000, 1, "3x3_s1_uncontended"),
    (9, 1700, 4, "9x9_s4"),
    (9, 1700, 1, "9x9_s1_uncontended"),
]


def run_one(cdim, cap, streams, label, n_frames, batch, outdir) -> dict | None:
    rep = outdir / f"probe_{label}_cap{cap}"
    print(f"\n--- {label}: {cdim}x{cdim} cap={cap} streams={streams} N={n_frames} ---")

    prof = [common.NSYS, "profile", "--trace=cuda", "--sample=none",
            "--cpuctxsw=none", "--force-overwrite=true", "-o", str(rep),
            sys.executable, str(HERE / "nsys_kernel_probe.py"),
            str(streams), str(n_frames), str(cdim), str(cap), str(batch)]
    p = subprocess.run(prof, capture_output=True, text=True)
    for line in p.stdout.splitlines():
        if line.strip().startswith(("n_streams", "H2D/frame", "wall")):
            print("   ", line.strip())
    if not rep.with_suffix(".nsys-rep").exists():
        print(f"    FAILED: {p.stderr.strip()[-400:]}")
        return None

    subprocess.run([common.NSYS, "stats", "--force-export=true", "--report",
                    "cuda_gpu_sum", str(rep.with_suffix(".nsys-rep"))],
                   capture_output=True, text=True)
    sq = rep.with_suffix(".sqlite")
    if not sq.exists():
        print("    FAILED: no sqlite export")
        return None

    r = gpu_span.analyze(sq, n_frames)
    r.update(label=label, cluster_dim=cdim, cap=cap, n_streams=streams,
             batch=batch, device_ped_type=common.device_ped_type())
    print(f"    kernel {r['kernel_us_per_frame']:5.2f} us (duty {r['kernel_duty_pct']:4.1f}%)  "
          f"H2D {r['H2D_us_per_frame']:5.2f} ({r['H2D_duty_pct']:4.1f}%)  "
          f"D2H {r['D2H_us_per_frame']:5.2f} ({r['D2H_duty_pct']:4.1f}%)")
    print(f"    -> roofline: {r['bottleneck']}-bound at "
          f"{r['roofline_us_per_frame']:.2f} us/frame = {r['roofline_fps']:,.0f} FPS")
    return r


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--frames", type=int, default=20_000)
    ap.add_argument("--batch", type=int, default=2000)
    ap.add_argument("--tag", default="")
    ap.add_argument("--only", nargs="+", default=None, help="subset of labels")
    ap.add_argument("--cap", type=int, default=None, help="override every cap")
    args = ap.parse_args()

    common.assert_build_fresh()
    common.assert_idle_gpu()
    env = common.capture_env()
    outdir = common.results_dir(args.tag)
    common.write_env(outdir / "env.json", env)
    print(f"build: DEVICE_PED_TYPE={env['device_ped_type']}  git={env['git_rev']}"
          f"{' (dirty)' if env['git_dirty'] else ''}")
    print(f"out:   {outdir}")

    rows = []
    for cdim, cap, streams, label in CONFIGS:
        if args.only and label not in args.only:
            continue
        r = run_one(cdim, args.cap or cap, streams, label, args.frames, args.batch, outdir)
        if r:
            rows.append(r)

    if not rows:
        return 1
    out = outdir / "probes.csv"

    def key(r):
        return (str(r["label"]), str(r["cap"]), str(r["n_streams"]))

    prior = list(csv.DictReader(out.open())) if out.exists() else []
    fresh = {key(r) for r in rows}
    merged = [r for r in prior if key(r) not in fresh] + rows
    with out.open("w", newline="") as fh:
        w = csv.DictWriter(fh, fieldnames=list(rows[0]))
        w.writeheader()
        w.writerows(merged)

    print(f"\n=== rooflines ({env['device_ped_type']} build, us/frame) ===")
    print(f"{'config':<22} {'kernel':>8} {'H2D':>8} {'D2H':>8}  "
          f"{'bottleneck':<10} {'roofline':>9} {'FPS':>10}")
    for r in rows:
        print(f"{r['label']:<22} {r['kernel_us_per_frame']:8.2f} "
              f"{r['H2D_us_per_frame']:8.2f} {r['D2H_us_per_frame']:8.2f}  "
              f"{r['bottleneck']:<10} {r['roofline_us_per_frame']:9.2f} "
              f"{r['roofline_fps']:10,.0f}")
    print(f"\n-> {out}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
