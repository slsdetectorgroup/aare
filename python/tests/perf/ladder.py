"""The opt1 → opt8 ladder as a configuration matrix.

The point of this file is that the ladder is *data*, not code. Every step runs
through the same measure() function, so nothing can differ between two steps
except the fields in their STEP entry. Previous campaigns drifted precisely
because each step lived in its own notebook cell with its own warm-up policy.

Three classes cover eight steps:

    ClusterFinderCUDAOpt2   opt1, opt2   frozen pre-refactor pipeline (88e0e8d)
    ClusterFinderCUDAGraph  opt5         graph-based
    ClusterFinderCUDA       opt3, opt4, opt7, opt8

opt3/opt4 are reachable on the *current* ClusterFinderCUDA because batch_chunk
disables the internal chunking that opt7 added: set it to the batch size and
find_clusters_batched degenerates to submit-everything-then-collect-everything.
This is why the old pipeline does not need to be kept alive in a second class.

opt6 is NOT a row here — it is the DEVICE_PED_TYPE build axis, so the whole
matrix is run once per build and the two result sets are compared.
"""
from __future__ import annotations

import gc
import sys
import time
from dataclasses import dataclass, field
from typing import Callable

import numpy as np

import common
from common import Row, faults, load_frames, image_size, train_pedestal


@dataclass(frozen=True)
class Step:
    step: str
    label: str
    cls: str  # "opt2cls" | "graph" | "cuda"
    n_streams: int = 4
    pinned: bool = True
    batch_chunk: str = "auto"  # "auto" | "off" | int
    collection: str = "collect"
    per_frame: bool = False  # opt1: one find_clusters() call per frame
    act: str = ""
    status: str = "adopted"
    # ClusterFinderCUDAOpt2 is registered for 3x3 only (cuda_bindings.cu), so
    # the 9x9 ladder necessarily starts at opt3. Not a limitation of the
    # measurement — the deck's arc is the 3x3 one.
    cluster_dims: tuple[int, ...] = (3, 5, 7, 9)


# ---- the ladder ----------------------------------------------------------
STEPS: list[Step] = [
    # ClusterFinderMT cannot restart after stop(), so this is ALWAYS a
    # first-pass number carrying its own ~1.8 s of allocator faults (docs §3.4).
    # measure() forces reps=1 for it. Every speedup quoted against it must say
    # whether it uses the raw or the fault-corrected baseline.
    Step("cpu", "ClusterFinderMT", "cpu",   # thread count appended per size
         n_streams=0, pinned=False, batch_chunk="n/a", collection="n/a",
         act="baseline", status="reference"),
    Step("opt1", "1 stream, one launch per frame", "opt2cls",
         n_streams=1, pinned=False, collection="per-frame", per_frame=True,
         act="I - getting frames to the GPU", cluster_dims=(3,)),
    Step("opt2", "4 streams + host-side batching", "opt2cls",
         n_streams=4, pinned=False, act="I - getting frames to the GPU",
         cluster_dims=(3,)),
    Step("opt3", "pipeline rework, no pinning", "cuda",
         pinned=False, batch_chunk="off", act="I - getting frames to the GPU"),
    Step("opt4", "+ pinned input (DMA H2D)", "cuda",
         pinned=True, batch_chunk="off", act="I - getting frames to the GPU"),
    Step("opt5", "CUDA Graphs", "graph",
         pinned=True, act="I - getting frames to the GPU", status="rejected"),
    Step("opt7", "host<->GPU overlap, chunked internally", "cuda",
         pinned=True, batch_chunk="auto", act="III - getting results back"),
    Step("opt8", "zero-copy collection (collect_view)", "cuda",
         pinned=True, batch_chunk="auto", collection="collect_view",
         act="III - getting results back"),
]

STEPS_BY_NAME = {s.step: s for s in STEPS}

# The CPU baseline's ClusterCollector must outlive the finder's worker threads
# but cannot be attached to it — pybind11 classes have no __dict__, and the
# AttributeError from trying leaves ClusterFinderMT half-built with 48 threads
# running, which aborts the process at destruction. Only one CPU step runs at a
# time, so a module-level holder is sufficient and explicit.
_cpu_sink = None

# Frames whose returned cluster count landed exactly ON the cap. The kernel bumps
# its counter for every detection but guards only the write
# (clusterfinder_kernel.cuh: `if (write_idx >= max_clusters) return;`) and the host
# then clamps n_found to the cap, so a truncated frame is indistinguishable from a
# frame that happened to contain exactly `cap` clusters -- and silently short.
# With the cap set well above the observed maximum, landing on it is effectively
# impossible by chance, so this counter is a sound truncation detector. It exists
# because a cap that was believed non-truncating silently discarded 0.0095 % of
# 9x9 clusters through an entire campaign.
_at_cap = 0

# Best ClusterFinderMT thread count per cluster size, swept 2026-08-19
# (results/2026-08-19_cpu_threads/). This is a 16-core / 32-thread 7950X: the
# campaign's original n_threads=48 oversubscribed it by 1.5x and understated the
# CPU by 24 % at 3x3 and 15 % at 9x9, inflating every speedup. The optima differ
# by size because ClusterCollector's drain is inside the timed region and 9x9
# clusters are 9x larger.
CPU_THREADS = {3: 24, 9: 32}


# ---- finder construction -------------------------------------------------
def build_finder(step: Step, cluster_dim: int, cap: int, batch_size: int):
    """A FRESH finder, trained on the same 1000 pedestal frames.

    Never reuse one across steps: the device pedestal keeps evolving, so two
    steps sharing a finder are not measuring the same thing.
    """
    from aare import ClusterFinderCUDA, ClusterFinderCUDAGraph, ClusterFinderCUDAOpt2

    dim = (cluster_dim, cluster_dim)
    common_kw = dict(image_size=image_size(), cluster_size=dim, n_sigma=common.N_SIGMA,
                     max_clusters_per_frame=cap, n_streams=step.n_streams)

    if step.cls == "cpu":
        from aare import ClusterFinderMT, ClusterCollector

        global _cpu_sink
        cf = ClusterFinderMT(image_size(), dim, n_sigma=common.N_SIGMA,
                             capacity=cap,
                             n_threads=CPU_THREADS[cluster_dim])
        _cpu_sink = ClusterCollector(cf)
        train_pedestal(cf)
        return cf

    if step.cls == "opt2cls":
        # time_kernels kept OFF for comparability with the ClusterFinderCUDA
        # steps, which default to off. Without this, opt1/opt2 pay an event tax
        # that opt3+ do not, inflating the opt2 -> opt3 step (docs §11).
        cf = ClusterFinderCUDAOpt2(**common_kw, time_kernels=False)
    elif step.cls == "graph":
        cf = ClusterFinderCUDAGraph(**common_kw)
    elif step.cls == "cuda":
        cf = ClusterFinderCUDA(**common_kw, time_kernels=False)
    else:
        raise ValueError(step.cls)

    train_pedestal(cf)

    if step.cls == "cuda":
        cf.batch_chunk = batch_size if step.batch_chunk == "off" else (
            0 if step.batch_chunk == "auto" else int(step.batch_chunk))
    return cf


# ---- the measured region -------------------------------------------------
def steps_for(cluster_dim: int) -> list[Step]:
    """The steps that can actually run at this cluster size."""
    return [s for s in STEPS if cluster_dim in s.cluster_dims]


def measure(step: Step, cluster_dim: int, cap: int, n_frames: int,
            batch_size: int, reps: int, retain: bool = False) -> list[Row]:
    """Run one ladder step `reps` times, returning one Row per rep.

    Policy applied identically to every step:
      - fresh finder, same pedestal
      - input pinned only if the step says so (that IS opt4)
      - output slots pre-pinned OUTSIDE the timer, without processing frames
      - faults bracketed around the timed region only

    Reps deliberately SHARE one finder, because the whole point of repeating is
    to reach the heap plateau, and a new finder would reset it. Two consequences
    to keep in mind when reading the CSV:

      * quote the last rep, not the mean — earlier reps carry first-touch faults;
      * n_clusters drifts by ~0.002 % between reps, because the device pedestal
        advances by n_frames each pass. This is expected and is why steps are
        compared against each other at the same rep index, never across reps.
    """
    data = load_frames(n_frames)
    rows: list[Row] = []
    if step.cls == "cpu":
        reps = 1  # stop() is terminal; a second pass is impossible
    cf = build_finder(step, cluster_dim, cap, batch_size)

    if step.pinned:
        cf.register_input_buffer(data)

    # Pre-pin the output slots. Allocation only: no transfer, no launch, and
    # crucially no pedestal advance, so this cannot perturb the result. Sized to
    # the largest batch this step will submit (docs §12.1).
    if hasattr(cf, "reserve_output_slots"):
        slot_frames = batch_size if step.batch_chunk == "off" else (
            cf.chunk_size_for(min(n_frames, batch_size))
            if hasattr(cf, "chunk_size_for") else batch_size)
        cf.reserve_output_slots(slot_frames)

    try:
        rows = _reps(cf, step, cluster_dim, cap, n_frames, batch_size, reps,
                     retain, data)
    finally:
        # A finder that escapes here still owns worker threads (CPU) or CUDA
        # streams, and destroying it implicitly aborts the process. Release it
        # explicitly whatever happened.
        if step.pinned:
            try:
                cf.unregister_input_buffer()
            except Exception:
                pass
        del cf
        gc.collect()
    return rows


def _reps(cf, step: Step, cluster_dim: int, cap: int, n_frames: int,
          batch_size: int, reps: int, retain: bool, data) -> list[Row]:
    global _at_cap
    rows: list[Row] = []
    for rep in range(reps):
        gc.collect()
        _at_cap = 0
        mf0, Mf0 = faults()
        t0 = time.perf_counter()
        n_clusters = _drive(cf, step, data, n_frames, batch_size, cap, retain)
        wall = time.perf_counter() - t0
        mf1, Mf1 = faults()

        if _at_cap:
            print(f"  !! {step.step} {cluster_dim}x{cluster_dim} rep {rep}: "
                  f"{_at_cap} frame(s) returned exactly cap={cap} clusters — "
                  f"the cap is truncating and this row undercounts. Raise it.",
                  file=sys.stderr, flush=True)

        # The CPU baseline's thread count varies by cluster size, so it goes in
        # the label rather than being hardcoded: a row must say what it ran.
        label = (f"{step.label}, {CPU_THREADS[cluster_dim]} threads"
                 if step.cls == "cpu" else step.label)
        rows.append(Row(
            step=step.step, label=label, cluster_dim=cluster_dim, cap=cap,
            n_frames=n_frames, n_streams=step.n_streams, pinned=step.pinned,
            batch_chunk=step.batch_chunk, collection=step.collection,
            device_ped_type=common.device_ped_type(), rep=rep,
            wall_s=wall, us_per_frame=wall * 1e6 / n_frames,
            fps=n_frames / wall, minor_faults=mf1 - mf0, major_faults=Mf1 - Mf0,
            n_clusters=n_clusters, clusters_per_frame=n_clusters / n_frames,
            notes=" ".join(filter(None, [
                step.status if step.status != "adopted" else "",
                "retain" if retain else "",
                f"TRUNCATED at_cap={_at_cap}" if _at_cap else "",
            ])),
        ))

    return rows


def _drive(cf, step: Step, data, n_frames: int, batch_size: int, cap: int,
           retain: bool = False) -> int:
    """The timed work. One branch per collection strategy, nothing else.

    `retain=False` (default) counts each batch and lets it die, so peak result
    memory is one batch. This is what makes N=100 000 possible at 9x9, where
    retaining every ClusterVector would need 46.6 GB, and it is also the only
    mode in which opt8 is comparable — a BatchView cannot be retained.

    `retain=True` keeps everything, which is what the notebook does. It measures
    the finder PLUS a growing result heap; at 9x9 that heap is re-faulted every
    pass because it sits above glibc's mmap threshold (docs §12.2).
    """
    global _at_cap
    total = 0
    kept: list = [] if retain else None

    # The CPU finder's `cap` is a ClusterVector capacity, which grows on demand,
    # so it cannot truncate and is not checked.
    if step.cls == "cpu":
        for i in range(n_frames):
            cf.find_clusters(data[i])
        cf.stop()          # terminal — this is why the CPU step cannot be repped
        _cpu_sink.stop()
        for cv in _cpu_sink.steal_clusters():
            total += cv.size
        return total

    if step.per_frame:
        # opt1: no batching at all — one find_clusters() call per frame, and the
        # result stolen out of the finder's internal vector each time. This is
        # the fully synchronous path: H2D, kernel and D2H cannot overlap because
        # there is only one frame in flight.
        for i in range(n_frames):
            cf.find_clusters(data[i], i)
            cv = cf.steal_clusters()
            total += cv.size
            _at_cap += cv.size == cap
            if retain:
                kept.append(cv)
        return total

    if step.collection == "collect_view":
        from aare import find_cluster_views_batched_iter

        for start in range(0, n_frames, batch_size):
            stop = min(start + batch_size, n_frames)
            for v in find_cluster_views_batched_iter(
                    cf, data[start:stop], first_frame=start):
                # No retain branch: a view is borrowed and released at the end of
                # this iteration. That constraint IS opt8.
                total += v.total_clusters
                _at_cap += int(np.count_nonzero(np.asarray(v.counts) == cap))
        return total

    for start in range(0, n_frames, batch_size):
        stop = min(start + batch_size, n_frames)
        batch = cf.find_clusters_batched(data[start:stop], first_frame=start)
        for cv in batch:
            total += cv.size
            _at_cap += cv.size == cap
        if retain:
            kept.extend(batch)
    return total
