#!/usr/bin/env python3
"""Unprofiled end-to-end throughput: wall-clock FPS, no nsys.

    python throughput.py                           # 3x3..9x9 at 1 and 4 streams
    python throughput.py --streams 4 --repeats 5 --tag mytag

The nsys probes give per-engine busy time, i.e. the roofline. Under the
profiler the wall clock is inflated, so the achieved rate is measured here.
Two host paths over the same frames:

  copy  cf.find_clusters_batched(): one owned ClusterVector per frame, the way
        the notebook and most callers consume the output.
  view  find_cluster_views_batched_iter(): zero-copy BatchViews; the host only
        reads the cluster counts. This is the GPU pipeline's own rate.

The input is pinned once and the output slots are reserved before timing, as
in nsys_kernel_probe.py. Each path gets one untimed pass (clock ramp, first
touch), then the median of --repeats timed passes. Output: one row per
(size, streams, path) in results/<date>_<f32|f64>_<tag>/throughput.csv.
"""
from __future__ import annotations

import argparse
import csv
import statistics
import sys
import time
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import cf_common  # noqa: E402  (puts ../ and the build dir on sys.path)
from cf_common import common  # noqa: E402

from aare import File, ClusterFinderCUDA, find_cluster_views_batched_iter  # noqa: E402

# (cluster_dim, cap): the caps of run_probes.py, so the D2H bytes match.
SIZES = [(3, 3000), (5, 2350), (7, 2170), (9, 1700)]


def one_pass(cf, data, batch, path) -> tuple[float, int]:
    n = 0
    t0 = time.perf_counter()
    for start in range(0, len(data), batch):
        chunk = data[start:start + batch]
        if path == "copy":
            for cv in cf.find_clusters_batched(chunk, first_frame=start):
                n += cv.size
        else:
            for v in find_cluster_views_batched_iter(cf, chunk, first_frame=start):
                n += v.total_clusters
    return time.perf_counter() - t0, n


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--frames", type=int, default=20_000)
    ap.add_argument("--batch", type=int, default=2000)
    ap.add_argument("--sizes", type=int, nargs="+", default=[c for c, _ in SIZES],
                    help="cluster dims to run, e.g. --sizes 5")
    ap.add_argument("--streams", type=int, nargs="+", default=[1, 4])
    ap.add_argument("--paths", nargs="+", choices=["copy", "view"],
                    default=["copy", "view"])
    ap.add_argument("--repeats", type=int, default=3)
    ap.add_argument("--tag", default="throughput")
    args = ap.parse_args()

    common.assert_build_fresh(cf_common.HEADERS)
    common.assert_idle_gpu()
    env = common.capture_env(device_ped_type=cf_common.device_ped_type())
    outdir = cf_common.results_dir(args.tag)
    common.write_env(outdir / "env.json", env)
    print(f"build: DEVICE_PED_TYPE={env['device_ped_type']}  git={env['git_rev']}"
          f"{' (dirty)' if env['git_dirty'] else ''}")
    print(f"out:   {outdir}")

    f = File(common.DATA_FILE)
    ped = File(common.PEDESTAL_FILE).read_n(cf_common.N_PEDESTAL_FRAMES)
    data = f.read_n(args.frames)

    rows = []
    print(f"\n{'config':<10} {'path':<5} {'FPS':>10} {'us/frame':>9} {'clusters/frame':>15}")
    for cdim, cap in SIZES:
        if cdim not in args.sizes:
            continue
        for ns in args.streams:
            cf = ClusterFinderCUDA((f.rows, f.cols), cluster_size=(cdim, cdim),
                                   n_sigma=cf_common.N_SIGMA,
                                   max_clusters_per_frame=cap, n_streams=ns)
            for frame in ped:
                cf.push_pedestal_frame(frame.copy())
            cf.register_input_buffer(data)
            cf.reserve_output_slots(cf.chunk_size_for(min(args.frames, args.batch)))
            for path in args.paths:
                one_pass(cf, data, args.batch, path)
                runs = [one_pass(cf, data, args.batch, path) for _ in range(args.repeats)]
                t = statistics.median(r[0] for r in runs)
                row = dict(label=f"{cdim}x{cdim}_s{ns}", cluster_dim=cdim, cap=cap,
                           n_streams=ns, path=path, batch=args.batch,
                           n_frames=args.frames, repeats=args.repeats, wall_s=t,
                           fps=args.frames / t, us_per_frame=t * 1e6 / args.frames,
                           clusters_per_frame=runs[-1][1] / args.frames,
                           device_ped_type=env["device_ped_type"])
                rows.append(row)
                print(f"{row['label']:<10} {path:<5} {row['fps']:10,.0f} "
                      f"{row['us_per_frame']:9.2f} {row['clusters_per_frame']:15.2f}")
            cf.unregister_input_buffer()
            del cf

    out = outdir / "throughput.csv"
    with out.open("w", newline="") as fh:
        w = csv.DictWriter(fh, fieldnames=list(rows[0]))
        w.writeheader()
        w.writerows(rows)
    print(f"\n-> {out}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
