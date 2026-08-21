#!/usr/bin/env python3
"""Run the opt1 → opt8 ladder and write one CSV row per (step, rep).

    python run_ladder.py --dry-run            # 2000 frames, 1 rep, both sizes
    python run_ladder.py                      # the real campaign
    python run_ladder.py --dims 9 --reps 3

Output lands in perf/results/<date>_<f32|f64>/ together with env.json and a
manifest row, so every number in the report can be traced back to the run that
produced it and the build it was taken on.

The build axis (opt6) is NOT a command-line option: it is compiled in. Check
env.json's device_ped_type to know which arm a result set belongs to.
"""
from __future__ import annotations

import argparse
import csv
import subprocess
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))

import common
import ladder
from common import Row


# Per-cluster-size configuration. FIXED for the campaign. Optimising over these
# is a separate exercise; what matters here is that every step sees the same ones.
#
#   cap        MEASURED against the per-frame maximum over each campaign block,
#              not guessed. With a probing cap high enough never to bind:
#
#                             frames     mean      max    cap    truncates?
#                  3x3       100 000   2 330.9   2 545   3000    no
#                  9x9        20 000   1 422.1   1 633   1700    no
#
#              The 9x9 cap was 1500 for the whole earlier campaign, which is
#              BELOW the maximum: it silently dropped 2 715 clusters (0.0095 %)
#              across 64 frames, because the kernel guards only the write and the
#              host clamps the count. ladder.py now detects this — any frame that
#              returns exactly `cap` clusters is flagged and the row is marked
#              TRUNCATED — so the constant can never again be quietly wrong.
#
#              What raising it costs, MEASURED (results/2026-08-20_{f32,f64}_capAB):
#              D2H copies the whole fixed-size slot (4 + cap * sizeof(ClusterType))
#              regardless of occupancy, so at 9x9 the cap is a throughput knob.
#              1500 -> 1700 grows the slot 480.5 -> 544.5 KiB and D2H 22.8 -> 25.2
#              us/frame on BOTH arms (the payload is int32 either way). The cost
#              is arm-dependent, because it depends what the extra bytes hide under:
#
#                  arm   kernel@s4   D2H@1700   binds     opt8 cost of the cap
#                  f64      32.66      25.25    kernel      +0.1 %  (free)
#                  f32      23.94      25.24    D2H         -5.9 %
#
#              i.e. opt7's -40 % kernel is exactly what makes the cap expensive:
#              it drops the kernel below the enlarged D2H bar. At 3x3 the cluster
#              is 40 B and the cap is free at any sane value.
#   n_streams  4 everywhere. Campaign C used 8 at 9x9; §8 then showed 8 streams
#              buy no kernel concurrency there (instance time +1%) while
#              inflating the event timer 3.5x. Fixed at 4 and re-measured.
#   batch      2000 everywhere. The §10 opt7 measurement used 3000; nothing else did.
#   n_frames   100k at 3x3, 20k at 9x9 — the historical Campaign A and C values.
#              9x9 is held at 20k because the result heap is ~5x larger per frame
#              (1422 x 328 B = 466 kB vs 2330 x 40 B = 93 kB), so 100k would need
#              46.6 GB to retain against 98 GB free with no swap. At 20k it is
#              9.3 GB, which keeps --retain feasible at both sizes.
CONFIGS = {
    3: dict(cap=3000, n_frames=100_000, batch_size=2000, n_streams=4),
    9: dict(cap=1700, n_frames=20_000, batch_size=2000, n_streams=4),
}

# Sustained per-frame roofline = tallest engine bar, from the nsys probes.
# Used only to annotate the printed table; never written to the CSV, because a
# roofline is a derived quantity and belongs in the report, not the raw data.
ROOFLINE_US = {3: 16.2, 9: 23.9}


def _run_isolated(step_name: str, dim: int, args, outdir: Path) -> list[Row]:
    """Run one step in a FRESH process and read its rows back.

    Required for the fault columns to mean anything: the heap is process-wide,
    so in a shared process every step inherits what the previous ones grew.
    The dataset is re-read per step, which is the cost of the isolation.
    """
    tmp = outdir / f".{step_name}_{dim}.csv"
    cmd = [sys.executable, str(Path(__file__).resolve()),
           "--dims", str(dim), "--steps", step_name, "--reps", str(args.reps),
           "--_worker-out", str(tmp), "--allow-busy-gpu"]
    if args.frames:
        cmd += ["--frames", str(args.frames)]
    if args.retain:
        cmd += ["--retain"]
    proc = subprocess.run(cmd, capture_output=True, text=True)
    if not tmp.exists():
        raise RuntimeError(
            f"worker for {step_name} produced nothing "
            f"(exit {proc.returncode}): {proc.stderr.strip()[-400:]}")
    rows = []
    for d in csv.DictReader(tmp.open()):
        for k, f in Row.__dataclass_fields__.items():
            if f.type in ("int", int):
                d[k] = int(d[k])
            elif f.type in ("float", float):
                d[k] = float(d[k])
            elif f.type in ("bool", bool):
                d[k] = d[k] == "True"
        rows.append(Row(**d))
    tmp.unlink()
    return rows


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--dims", type=int, nargs="+", default=[3, 9],
                    help="cluster sizes to run (default: 3 9)")
    ap.add_argument("--reps", type=int, default=5,
                    help="repetitions per step. 5 because collect() is bistable and 3 cannot separate a plateau from an oscillation (default: 5)")
    ap.add_argument("--frames", type=int, default=None,
                    help="override n_frames for every size")
    ap.add_argument("--steps", nargs="+", default=None,
                    help="subset of step names, e.g. --steps opt7 opt8")
    ap.add_argument("--tag", default="", help="suffix for the results directory")
    ap.add_argument("--dry-run", action="store_true",
                    help="2000 frames, 1 rep — proves the matrix executes")
    ap.add_argument("--allow-busy-gpu", action="store_true",
                    help="skip the idle-GPU check (results will not be quotable)")
    ap.add_argument("--no-isolate", action="store_true",
                    help="run every step in ONE process. Faster (the dataset is "
                         "loaded once) but fault counts become meaningless: the "
                         "heap is process-wide, so each step inherits whatever "
                         "the previous ones grew. Measured: opt3 reports 2 faults "
                         "after opt1/opt2 have run, and 92,251 on its own. "
                         "Throughput is unaffected either way.")
    ap.add_argument("--_worker-out", default=None,
                    help=argparse.SUPPRESS)  # internal: one step, write CSV, exit
    ap.add_argument("--retain", action="store_true",
                    help="keep every ClusterVector instead of discarding each "
                         "batch after counting. Measures the finder PLUS a "
                         "growing result heap, which is what the notebook did. "
                         "Needs N<=20000 at 9x9 or it will OOM (46.6 GB at 100k).")
    args = ap.parse_args()

    if args.dry_run:
        args.frames, args.reps = 2000, 1
        args.tag = args.tag or "dryrun"

    if not args.allow_busy_gpu and not getattr(args, "_worker_out", None):
        common.assert_build_fresh()
        common.assert_idle_gpu()

    env = common.capture_env()
    outdir = common.results_dir(args.tag)
    common.write_env(outdir / "env.json", env)

    print(f"build: DEVICE_PED_TYPE={env['device_ped_type']}  "
          f"git={env['git_rev']}{'+dirty' if env['git_dirty'] else ''}  "
          f"gpu={env['gpu']}")
    print(f"out:   {outdir}\n")

    for dim in args.dims:
        cfg = dict(CONFIGS[dim])
        if args.frames:
            cfg["n_frames"] = args.frames
        steps = ladder.steps_for(dim)
        if args.steps:
            steps = [s for s in steps if s.step in args.steps]
        if not steps:
            print(f"{dim}x{dim}: no runnable steps, skipping")
            continue

        print(f"=== {dim}x{dim}  cap={cfg['cap']}  N={cfg['n_frames']:,}  "
              f"batch={cfg['batch_size']}  reps={args.reps} ===")
        skipped = [s.step for s in ladder.STEPS if s not in steps]
        if skipped and not args.steps:
            print(f"    not available at this size: {', '.join(skipped)}")

        rows: list[Row] = []
        for step in steps:
            try:
                if args.no_isolate or getattr(args, "_worker_out", None):
                    got = ladder.measure(step, dim, cfg["cap"], cfg["n_frames"],
                                         cfg["batch_size"], args.reps,
                                         retain=args.retain)
                else:
                    got = _run_isolated(step.step, dim, args, outdir)
            except Exception as exc:  # one broken step must not lose the rest
                print(f"    {step.step:<6} FAILED: {type(exc).__name__}: {exc}")
                continue
            if not got:
                print(f"    {step.step:<6} produced no rows")
                continue
            rows.extend(got)
            last = got[-1]
            print(f"    {step.step:<6} {last.fps:9,.0f} FPS  "
                  f"{last.us_per_frame:6.1f} us/f  faults {last.minor_faults:>9,}")

        if not rows:
            continue
        if getattr(args, "_worker_out", None):
            common.write_rows(Path(getattr(args, "_worker_out", None)), rows)
            continue
        csv_path = outdir / f"ladder_{dim}x{dim}.csv"
        common.write_rows(csv_path, rows)
        common.append_manifest(outdir / "manifest.csv", {
            "artifact": csv_path.name,
            "kind": "end-to-end wall/FPS",
            "config": f"{dim}x{dim} cap={cfg['cap']} N={cfg['n_frames']} "
                      f"batch={cfg['batch_size']} streams={cfg['n_streams']} "
                      f"consumer={'retain' if args.retain else 'streaming'}",
            "build": env["device_ped_type"],
            "cites": "docs/ClusterFinderCUDA_benchmark_results.md §5, §6, §7, §8, §9, §11",
            "produced_by": "perf/run_ladder.py",
            "timestamp": env["timestamp"],
        })

        print(f"\n    -> {csv_path.name}   (all reps; cold = rep 0, warm = last)")
        common.print_table(rows, ROOFLINE_US.get(dim))
        print()

    print(f"manifest: {outdir / 'manifest.csv'}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
