"""How many threads should the CPU baseline actually use?

The campaign's CPU reference was ClusterFinderMT with n_threads=48. This machine
is a Ryzen 9 7950X: 16 physical cores, 32 logical. 48 threads oversubscribes it
by 1.5x, so the baseline was slower than the CPU can go and every GPU speedup
quoted against it was correspondingly flattered.

This sweeps the thread count at both cluster sizes and reports the best. The
result is the number the deck and the report should divide by.

Two timings are recorded per point, because the campaign and the notebook do not
measure the same thing:

  loop_s  -- the find_clusters() loop alone. This is what
             ClusterFinderCUDA_perf.ipynb prints as `CPU clustering`.
  wall_s  -- loop + stop() + draining the ClusterCollector, which is what
             ladder.py's `cpu` step records, because _drive() does the drain
             inside the timed region.

wall_s is the one to compare against the GPU rows in ladder_*.csv: those include
their own result collection. loop_s is here so the notebook's number can be
reconciled with this one rather than looking like a contradiction.

Matches the ladder's CPU step in every other respect: same 1000 pedestal frames,
same caps, same frame counts, a fresh finder per point, clusters retained.

    python python/tests/perf/cpu_threads.py [--tag 2026-08-19_cpu]
"""
from __future__ import annotations

import argparse
import gc
import sys
import time
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))

import common
from common import Row, faults

# 8   = half the physical cores
# 16  = one per physical core
# 24  = 1.5x cores  (the count that beat 48 in the notebook)
# 32  = one per logical thread
# 48  = the campaign's original, 1.5x oversubscribed
THREADS = [8, 16, 24, 32, 48]

# (cluster_dim, cap, n_frames) -- exactly the ladder's `cpu` row for each size.
# 9x9 ran at 20 000 frames there, so it stays at 20 000 here.
CONFIGS = [(3, 3000, 100_000), (9, 1500, 20_000)]


def measure(dim: int, cap: int, n_frames: int, n_threads: int, data) -> Row:
    """One point. The finder is built, trained, driven and destroyed here.

    ClusterFinderMT.stop() is terminal, so a point cannot be repeated on the
    same finder -- which is also why the ladder runs the CPU step with reps=1.
    """
    from aare import ClusterFinderMT, ClusterCollector

    cf = ClusterFinderMT(common.image_size(), (dim, dim), n_sigma=common.N_SIGMA,
                         capacity=cap, n_threads=n_threads)
    sink = ClusterCollector(cf)
    common.train_pedestal(cf)

    gc.collect()
    mf0, Mf0 = faults()
    t0 = time.perf_counter()
    for i in range(n_frames):
        cf.find_clusters(data[i])
    loop_s = time.perf_counter() - t0

    # The drain is inside ladder.py's timed region, so it is inside ours too.
    cf.stop()
    sink.stop()
    n_clusters = 0
    for cv in sink.steal_clusters():
        n_clusters += cv.size
    wall_s = time.perf_counter() - t0
    mf1, Mf1 = faults()

    del cf, sink
    gc.collect()

    return Row(
        step="cpu", label=f"ClusterFinderMT, {n_threads} threads",
        cluster_dim=dim, cap=cap, n_frames=n_frames, n_streams=0, pinned=False,
        batch_chunk="n/a", collection="n/a",
        device_ped_type=common.device_ped_type(), rep=0,
        wall_s=wall_s, us_per_frame=wall_s * 1e6 / n_frames,
        fps=n_frames / wall_s, minor_faults=mf1 - mf0, major_faults=Mf1 - Mf0,
        n_clusters=n_clusters, clusters_per_frame=n_clusters / n_frames,
        notes=f"retain threads={n_threads} loop_s={loop_s:.3f} "
              f"loop_fps={n_frames / loop_s:.1f}",
    )


def main() -> None:
    ap = argparse.ArgumentParser()
    ap.add_argument("--tag", default=time.strftime("%Y-%m-%d") + "_cpu")
    ap.add_argument("--threads", type=int, nargs="*", default=THREADS)
    args = ap.parse_args()

    out = common.results_dir(args.tag)
    rows: list[Row] = []

    for dim, cap, n_frames in CONFIGS:
        data = common.load_frames(n_frames)
        print(f"\n=== {dim}x{dim}, cap {cap}, {n_frames:,} frames "
              f"{'=' * 30}", flush=True)
        print(f"{'threads':>8} {'wall_s':>9} {'FPS':>10} {'us/fr':>9} "
              f"{'loop FPS':>10} {'faults':>12}", flush=True)
        for n_threads in args.threads:
            r = measure(dim, cap, n_frames, n_threads, data)
            rows.append(r)
            loop_fps = float(r.notes.split("loop_fps=")[1])
            print(f"{n_threads:>8} {r.wall_s:>9.3f} {r.fps:>10,.1f} "
                  f"{r.us_per_frame:>9.1f} {loop_fps:>10,.1f} "
                  f"{r.minor_faults:>12,}", flush=True)

    common.write_rows(out / "cpu_threads.csv", rows)
    common.write_env(out / "env.json", common.capture_env())
    common.append_manifest(out / "manifest.csv", dict(
        artifact="cpu_threads.csv", kind="ladder",
        config="ClusterFinderMT thread sweep, 3x3 + 9x9",
        build=common.device_ped_type(),
        cites="CPU baseline for every speedup in deck + report",
        produced_by="cpu_threads.py",
        timestamp=time.strftime("%Y-%m-%d %H:%M:%S")))

    print(f"\nwrote {out / 'cpu_threads.csv'}")
    print("\nbest per size (wall_s convention, comparable to ladder GPU rows):")
    for dim, _, _ in CONFIGS:
        best = max((r for r in rows if r.cluster_dim == dim), key=lambda r: r.fps)
        old = {3: 5228.6, 9: 1304.0}[dim]
        print(f"  {dim}x{dim}: {best.label:<32} {best.fps:>9,.1f} FPS  "
              f"({best.us_per_frame:6.1f} us/fr)   "
              f"vs 48-thread {old:,.1f} = {best.fps / old:.2f}x")


if __name__ == "__main__":
    main()
