#!/usr/bin/env python3
"""nsys probe sweep: per-engine GPU times, duty cycles and the roofline.

    python run_probes.py                 # the campaign sweep
    python run_probes.py --frames 2000   # quick check

Produces, per config, an .nsys-rep + .sqlite in the results directory and one
row in probes.csv. This is the ONLY source of the rooflines that run_ladder.py's
"% of roofline" column divides by.

Why this cannot be merged with run_ladder.py: nsys inflates wall clock ~4x by
tracing every CUDA API call, so a profiled run cannot produce a throughput
number, and an unprofiled run cannot produce a per-engine breakdown. Two tools,
two questions.

Why 20 000 frames and not 2 000: over a short run the GPU clocks never fully
ramp (210 MHz idle -> 3.1 GHz boost), which under-reports the GPU by ~10 %. Every
retained probe from the previous campaign used 2 000 frames, which is how a
26.7 us/frame roofline was published for a pipeline that sustains 23.9.
"""
from __future__ import annotations

import argparse
import csv
import subprocess
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))

import common
import gpu_span

HERE = Path(__file__).resolve().parent
NSYS = "/opt/nvidia/nsight-systems/2024.5.1/bin/nsys"

# (cluster_dim, cap, n_streams, label)
#   s4 = the configuration the ladder runs, so its roofline is the one to quote.
#        Caps MUST track run_ladder.py's CONFIGS. At 9x9 the D2H slot is
#        4 + cap * sizeof(ClusterType) and is copied whole regardless of
#        occupancy, so a probe at a different cap measures a different D2H bar
#        and its "roofline" would not be the ladder's.
#   s1 = the uncontended control: with one stream H2D and D2H never coexist, so
#        it separates "this engine is slow" from "these engines are fighting"
#        (docs §8.2 — H2D loses 23 % of its bandwidth against a busy D2H).
CONFIGS = [
    (3, 3000, 4, "3x3_s4"),
    (3, 3000, 1, "3x3_s1_uncontended"),
    (9, 1700, 4, "9x9_s4"),
    (9, 1700, 1, "9x9_s1_uncontended"),
]


def run_one(cdim, cap, streams, label, n_frames, batch, outdir) -> dict | None:
    # The cap is in the filename because at 9x9 it SETS the D2H bar: the slot is
    # 4 + cap * sizeof(ClusterType) and is copied whole regardless of occupancy.
    # Two probes of the same label at different caps are different measurements,
    # and the earlier campaign's 9x9 probes were taken at cap=1500. Without the
    # suffix they overwrite each other and the difference disappears.
    rep = outdir / f"probe_{label}_cap{cap}"
    print(f"\n--- {label}: {cdim}x{cdim} cap={cap} streams={streams} "
          f"N={n_frames} ---")

    prof = [NSYS, "profile", "--trace=cuda", "--sample=none", "--cpuctxsw=none",
            "--force-overwrite=true", "-o", str(rep),
            sys.executable, str(HERE / "nsys_kernel_probe.py"),
            str(streams), str(n_frames), str(cdim), str(cap), str(batch)]
    p = subprocess.run(prof, capture_output=True, text=True)
    for line in p.stdout.splitlines():
        if line.strip().startswith(("n_streams", "H2D/frame", "wall")):
            print("   ", line.strip())
    if not (rep.with_suffix(".nsys-rep")).exists():
        print(f"    FAILED: {p.stderr.strip()[-400:]}")
        return None

    # --force-export makes the .sqlite gpu_span.py reads
    subprocess.run([NSYS, "stats", "--force-export=true", "--report",
                    "cuda_gpu_sum", str(rep.with_suffix(".nsys-rep"))],
                   capture_output=True, text=True)
    sq = rep.with_suffix(".sqlite")
    if not sq.exists():
        print("    FAILED: no sqlite export")
        return None

    r = gpu_span.analyze(sq, n_frames)
    r.update(label=label, cluster_dim=cdim, cap=cap, n_streams=streams,
             batch=batch, device_ped_type=common.device_ped_type())
    print(f"    kernel {r['kernel_us_per_frame']:5.1f} us (duty {r['kernel_duty_pct']:4.1f}%)  "
          f"H2D {r['H2D_us_per_frame']:5.1f} ({r['H2D_duty_pct']:4.1f}%)  "
          f"D2H {r['D2H_us_per_frame']:5.1f} ({r['D2H_duty_pct']:4.1f}%)")
    print(f"    -> roofline: {r['bottleneck']}-bound at "
          f"{r['roofline_us_per_frame']:.1f} us/frame = {r['roofline_fps']:,.0f} FPS")
    return r


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--frames", type=int, default=20_000)
    ap.add_argument("--batch", type=int, default=2000)
    ap.add_argument("--tag", default="")
    ap.add_argument("--only", nargs="+", default=None, help="subset of labels")
    ap.add_argument("--cap", type=int, default=None,
                    help="override the cap for every selected config. At 9x9 the "
                         "cap sets the D2H bar (the slot is copied whole), so this "
                         "is how you A/B two caps ON ONE BUILD IN ONE SESSION "
                         "rather than against a probe taken days earlier. Artifact "
                         "filenames carry the cap, so runs do not overwrite.")
    args = ap.parse_args()

    common.assert_build_fresh()
    common.assert_idle_gpu()
    env = common.capture_env()
    outdir = common.results_dir(args.tag)
    common.write_env(outdir / "env.json", env)
    print(f"build: DEVICE_PED_TYPE={env['device_ped_type']}  git={env['git_rev']}")
    print(f"out:   {outdir}")

    rows = []
    for cdim, cap, streams, label in CONFIGS:
        if args.only and label not in args.only:
            continue
        if args.cap:
            cap = args.cap
        r = run_one(cdim, cap, streams, label, args.frames, args.batch, outdir)
        if r:
            rows.append(r)
            common.append_manifest(outdir / "manifest.csv", {
                "artifact": f"probe_{label}_cap{cap}.nsys-rep / .sqlite",
                "kind": "nsys per-engine GPU times + duty cycles",
                "config": f"{cdim}x{cdim} cap={cap} N={args.frames} "
                          f"streams={streams} batch={args.batch}",
                "build": env["device_ped_type"],
                "cites": "docs §7 rooflines, §8 kernel/memcpy, §8.1 duty cycles, §9",
                "produced_by": "perf/run_probes.py",
                "timestamp": env["timestamp"],
            })

    if rows:
        out = outdir / "probes.csv"
        # MERGE, do not clobber. A results directory may legitimately hold
        # several probe runs -- a cap A/B is exactly that -- and the artifact
        # filenames already carry the cap. Writing "w" here silently discarded
        # the first half of the first such A/B. Rows are keyed by
        # (label, cap, n_streams): re-running one config replaces its own row
        # and leaves every other row alone.
        def _key(r):
            return (str(r["label"]), str(r["cap"]), str(r["n_streams"]))

        prior = list(csv.DictReader(out.open())) if out.exists() else []
        fresh = {_key(r) for r in rows}
        merged = [r for r in prior if _key(r) not in fresh] + rows
        with out.open("w", newline="") as fh:
            w = csv.DictWriter(fh, fieldnames=list(rows[0]))
            w.writeheader()
            w.writerows(merged)
        print(f"\n=== rooflines ({env['device_ped_type']} build) ===")
        print(f"{'config':<22} {'kernel':>8} {'H2D':>8} {'D2H':>8}  "
              f"{'bottleneck':<10} {'roofline':>10} {'FPS':>10}")
        for r in rows:
            print(f"{r['label']:<22} {r['kernel_us_per_frame']:8.2f} "
                  f"{r['H2D_us_per_frame']:8.2f} {r['D2H_us_per_frame']:8.2f}  "
                  f"{r['bottleneck']:<10} {r['roofline_us_per_frame']:9.2f}u "
                  f"{r['roofline_fps']:10,.0f}")
        print(f"\n-> {out}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
