#!/usr/bin/env python3
"""Page faults and host memory on the ClusterFinderCUDA output path.

    python page_faults.py                                   # 20 000 frames, 3x3
    python page_faults.py --preheat-gb 3                    # pre-heat the heap first
    GLIBC_TUNABLES=glibc.malloc.mmap_threshold=33554432:glibc.malloc.trim_threshold=4294967295:glibc.malloc.top_pad=268435456 \\
        python page_faults.py --preheat-gb 3

Run it in a fresh process for every configuration: the state of the heap
(what is mapped, what glibc kept) carries over from one measurement to the
next, so a notebook kernel that has already run other cells cannot show a cold
start. The modes run in the order given, on the same frames:

  keep            find_clusters_batched(), every ClusterVector kept
                  (the notebook's Part 1 loop)
  keep+spectrum   the same, plus a spectrum filled per batch in Python
  drop+spectrum   spectrum filled per batch, then the batch is freed
  views+spectrum  find_cluster_views_batched_iter(): spectrum from v.sums(),
                  nothing allocated per frame

--preheat-gb N writes N GB of heap in 16 MiB blocks and frees it before the
first mode. glibc keeps those pages mapped only if it is told not to trim the
heap (GLIBC_TUNABLES above); without that the pre-heat is mostly given back.
The pre-heat takes about 0.1 s per GB when the machine has free memory (huge
pages); when it has to reclaim page cache first, 4 KiB pages and seconds per GB.

Faults are minor faults of this process (getrusage). When transparent huge
pages back the heap, one fault maps and zero-fills 2 MiB instead of 4 KiB, so
compare times, not only fault counts.
"""
from __future__ import annotations

import argparse
import gc
import os
import resource
import sys
import time
from pathlib import Path

import numpy as np

sys.path.insert(0, str(Path(__file__).resolve().parent))
import cf_common  # noqa: E402  (puts ../ and the build dir on sys.path)
from cf_common import common  # noqa: E402

import boost_histogram as bh  # noqa: E402
from aare import File, ClusterFinderCUDA, find_cluster_views_batched_iter  # noqa: E402

MODES = ["keep", "keep+spectrum", "drop+spectrum", "views+spectrum"]
N_BINS, E_MIN, E_MAX = 200, -2, 4000


def minor_faults() -> int:
    return resource.getrusage(resource.RUSAGE_SELF).ru_minflt


def smaps_kib(field: str) -> int:
    for line in Path("/proc/self/smaps_rollup").read_text().splitlines():
        if line.startswith(field + ":"):
            return int(line.split()[1])
    return 0


def preheat(gb: float, block: int = 16 << 20) -> None:
    """Grow the heap by `gb` GB, write every page, then free it all.

    np.empty is a plain malloc (calloc could skip the write on fresh pages),
    and 16 MiB stays under the 32 MiB mmap threshold, so the blocks come from
    the heap rather than from their own mappings."""
    blocks = []
    for _ in range(int(gb * (1 << 30)) // block):
        b = np.empty(block, np.uint8)
        b.fill(1)
        blocks.append(b)
    del blocks
    gc.collect()


def fill_spectrum(hist, vectors) -> None:
    energies = [np.asarray(cv.sum()).ravel() for cv in vectors if cv.size > 0]
    if energies:
        hist.fill(np.concatenate(energies))


def run(cf, data, batch, mode) -> list:
    hist = bh.Histogram(bh.axis.Regular(N_BINS, E_MIN, E_MAX))
    kept = []
    for start in range(0, len(data), batch):
        chunk = data[start:start + batch]
        if mode == "views+spectrum":
            for v in find_cluster_views_batched_iter(cf, chunk, first_frame=start):
                hist.fill(np.asarray(v.sums()).ravel())
            continue
        res = cf.find_clusters_batched(chunk, first_frame=start)
        if mode != "keep":
            fill_spectrum(hist, res)
        if mode.startswith("keep"):
            kept.extend(res)
        del res
    return kept  # freed by the caller, after the timer


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--frames", type=int, default=20_000)
    ap.add_argument("--batch", type=int, default=3000)
    ap.add_argument("--cluster", type=int, default=3, help="cluster size, e.g. 3 for 3x3")
    ap.add_argument("--cap", type=int, default=3000, help="max_clusters_per_frame")
    ap.add_argument("--streams", type=int, default=4)
    ap.add_argument("--preheat-gb", type=float, default=0.0)
    ap.add_argument("--modes", nargs="+", choices=MODES, default=MODES)
    args = ap.parse_args()

    common.assert_build_fresh(cf_common.HEADERS)
    common.assert_idle_gpu()

    tunables = os.environ.get("GLIBC_TUNABLES", "")
    thp = Path("/sys/kernel/mm/transparent_hugepage/enabled").read_text().split()
    thp = next((w.strip("[]") for w in thp if w.startswith("[")), "unknown")

    f = File(common.DATA_FILE)
    ped = File(common.PEDESTAL_FILE).read_n(cf_common.N_PEDESTAL_FRAMES)
    data = f.read_n(args.frames)

    cf = ClusterFinderCUDA((f.rows, f.cols), cluster_size=(args.cluster, args.cluster),
                           n_sigma=cf_common.N_SIGMA, max_clusters_per_frame=args.cap,
                           n_streams=args.streams,
                           min_pedestal_samples=cf_common.N_PEDESTAL_FRAMES)
    for frame in ped:
        cf.push_pedestal_frame(frame.copy())
    del ped
    cf.register_input_buffer(data)
    cf.reserve_output_slots(cf.chunk_size_for(min(args.frames, args.batch)))
    # Untimed warm-up that allocates nothing on the host: uploads the pedestal
    # and runs the first kernels, so the first mode is not charged for them.
    for v in find_cluster_views_batched_iter(cf, data[:args.batch]):
        v.total_clusters
    gc.collect()

    t_pre, pf_pre = 0.0, 0
    if args.preheat_gb > 0:
        pf0, t0 = minor_faults(), time.perf_counter()
        preheat(args.preheat_gb)
        t_pre, pf_pre = time.perf_counter() - t0, minor_faults() - pf0

    print(f"GLIBC_TUNABLES {'set' if tunables else 'not set'}, huge pages {thp}, "
          f"pre-heat {args.preheat_gb:g} GB"
          + (f" ({t_pre:.2f} s, {pf_pre:,} faults, untimed)" if args.preheat_gb else "")
          + f", {args.frames:,} frames, batch {args.batch}, "
          f"{args.cluster}x{args.cluster}")
    print(f"  {'mode':<16} {'s':>6} {'FPS':>9} {'faults':>10} {'faults/frame':>13}")
    for mode in args.modes:
        gc.collect()
        pf0, t0 = minor_faults(), time.perf_counter()
        kept = run(cf, data, args.batch, mode)
        t, pf = time.perf_counter() - t0, minor_faults() - pf0
        del kept
        print(f"  {mode:<16} {t:6.2f} {args.frames / t:9,.0f} {pf:10,} "
              f"{pf / args.frames:13.1f}")
    print(f"  resident {smaps_kib('Rss') / 2**20:.1f} GiB, of which in huge pages "
          f"{smaps_kib('AnonHugePages') / 2**20:.1f} GiB")

    cf.unregister_input_buffer()
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
