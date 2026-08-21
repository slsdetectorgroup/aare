"""Shared plumbing for the ClusterFinderCUDA measurement campaign.

Everything that could differ between ladder steps and silently change a number
lives here exactly once: dataset loading, pedestal training, fault bracketing,
slot pre-pinning and the CSV/manifest format. Steps differ only by the config
rows in ladder.py.

Two policies are enforced here rather than left to each caller, because both
have produced wrong numbers in this campaign before:

1. **Never warm up by processing frames.** The kernel pushes a pedestal update
   per pixel per frame, so a finder that has seen extra frames is no longer
   comparable with one that has not. Slots are pre-pinned with
   reserve_output_slots(), which allocates without transferring or launching.

2. **Every finder is fresh.** The device pedestal keeps evolving, so two steps
   sharing a finder see different pedestal states and cannot be compared.
"""
from __future__ import annotations

import csv
import json
import os
import resource
import subprocess
import sys
import time
from dataclasses import dataclass, asdict, field
from pathlib import Path

import numpy as np

REPO = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(REPO / "build"))

DATA_DIR = Path(
    "/mnt/sls_det_storage/moench_data/2603_MaxIVBeamtime/2026032408/process/xrf/"
)
DATA_FILE = DATA_DIR / "Cu_factor_10_data_master_0.json"
PEDESTAL_FILE = DATA_DIR / "Cu_factor_10_pedestal_master_0.json"

N_PEDESTAL_FRAMES = 1000
N_SIGMA = 5

# Cost of one first-touch page, measured on this machine (docs §3.1, §12).
# Two different values because pinned pages additionally cost driver work.
US_PER_HEAP_FAULT = 0.7
US_PER_PINNED_FAULT = 1.0


# --------------------------------------------------------------------------
# process-level instrumentation
# --------------------------------------------------------------------------
def faults():
    """(minor, major) fault counters. Bracket every timed region with these."""
    r = resource.getrusage(resource.RUSAGE_SELF)
    return r.ru_minflt, r.ru_majflt


def _sh(cmd, default="unknown"):
    try:
        return subprocess.run(
            cmd, shell=True, capture_output=True, text=True, timeout=30
        ).stdout.strip() or default
    except Exception:
        return default


def capture_env() -> dict:
    """Everything needed to know whether a later run is comparable to this one."""
    import aare

    return {
        "timestamp": time.strftime("%Y-%m-%d %H:%M:%S"),
        "host": _sh("hostname"),
        "git_rev": _sh(f"git -C {REPO} rev-parse --short HEAD"),
        "git_branch": _sh(f"git -C {REPO} rev-parse --abbrev-ref HEAD"),
        "git_dirty": bool(_sh(f"git -C {REPO} status --porcelain")),
        "aare_version": getattr(aare, "__version__", "unknown"),
        # The opt6 axis. Read from the header rather than trusted from memory:
        # a stale build is the single easiest way to mislabel a whole campaign.
        "device_ped_type": device_ped_type(),
        "gpu": _sh("nvidia-smi --query-gpu=name --format=csv,noheader"),
        "driver": _sh("nvidia-smi --query-gpu=driver_version --format=csv,noheader"),
        "gpu_busy_pct": _sh(
            "nvidia-smi --query-gpu=utilization.gpu --format=csv,noheader"
        ),
        "nvcc": _sh("nvcc --version | tail -1"),
        "python": sys.version.split()[0],
    }


def assert_build_fresh() -> None:
    """Abort if the installed extension predates the headers it was compiled from.

    device_ped_type() below parses the SOURCE header, so on an un-rebuilt tree it
    reports the type the source *claims* while the loaded module is still the
    previous build — and env.json is stamped with the wrong build identity. That
    is not hypothetical: on 2026-08-20 it produced a full-f64 9x9 probe labelled
    "float" (header edited 10:54, binary built 09:57). See
    results/2026-08-20_INVALID_stale_build/INVALID.md.

    mtime only. Cheap, and it catches the case that actually occurred.
    """
    so = list((REPO / "build" / "aare").glob("_aare_cuda*.so"))
    if not so:
        raise RuntimeError("assert_build_fresh: no _aare_cuda*.so under build/aare")
    built = max(f.stat().st_mtime for f in so)

    headers = [REPO / "include/aare/clusterfinder_kernel.cuh",
               REPO / "include/aare/ClusterFinderCUDA.hpp"]
    stale = [h for h in headers if h.exists() and h.stat().st_mtime > built]
    if stale:
        names = ", ".join(h.name for h in stale)
        raise RuntimeError(
            f"assert_build_fresh: {names} newer than the installed extension.\n"
            f"  header(s) edited after the build -> env.json would record the "
            f"SOURCE type, not the compiled one.\n"
            f"  Rebuild and reinstall before measuring.")


def device_ped_type() -> str:
    """Parse DEVICE_PED_TYPE out of the kernel header — the opt6 build axis.

    Reads SOURCE, not the binary; there is no binding that exposes the compiled
    type. Only meaningful once assert_build_fresh() has passed.
    """
    hdr = REPO / "include/aare/clusterfinder_kernel.cuh"
    try:
        for line in hdr.read_text().splitlines()[:40]:
            if "using DEVICE_PED_TYPE" in line and not line.strip().startswith("//"):
                return line.split("=")[1].split(";")[0].strip()
    except Exception:
        pass
    return "unknown"


def assert_idle_gpu(max_pct: int = 5):
    """A competing process leaves per-op averages intact while destroying the
    duty cycle and the wall clock (docs §14). Fail loudly rather than record it."""
    pct = _sh("nvidia-smi --query-gpu=utilization.gpu --format=csv,noheader")
    try:
        val = int(pct.split()[0])
    except Exception:
        return
    if val > max_pct:
        raise SystemExit(
            f"GPU is {val}% busy — another process is running. "
            f"Numbers taken now are not quotable (docs §14). Aborting."
        )


# --------------------------------------------------------------------------
# dataset + finders
# --------------------------------------------------------------------------
_data_cache: dict[int, np.ndarray] = {}


def load_frames(n_frames: int) -> np.ndarray:
    """Data frames, held in RAM so file I/O is outside every timing loop."""
    from aare import File

    if n_frames not in _data_cache:
        f = File(DATA_FILE)
        _data_cache[n_frames] = f.read_n(n_frames)
    return _data_cache[n_frames]


def image_size() -> tuple[int, int]:
    from aare import File

    f = File(DATA_FILE)
    return (f.rows, f.cols)


def train_pedestal(finder) -> None:
    """The same 1000 pedestal frames for every finder in every step."""
    from aare import File

    pd = File(PEDESTAL_FILE)
    for _ in range(N_PEDESTAL_FRAMES):
        finder.push_pedestal_frame(pd.read_frame().copy())


# --------------------------------------------------------------------------
# results
# --------------------------------------------------------------------------
@dataclass
class Row:
    """One measurement. Written verbatim to CSV; every report number cites one."""

    step: str  # opt1 … opt8, cpu
    label: str  # human-readable description
    cluster_dim: int
    cap: int
    n_frames: int
    n_streams: int
    pinned: bool
    batch_chunk: str  # "auto" | "off" | explicit int
    collection: str  # collect | collect_view | per-frame | n/a
    device_ped_type: str
    rep: int
    wall_s: float
    us_per_frame: float
    fps: float
    minor_faults: int
    major_faults: int
    n_clusters: int
    clusters_per_frame: float
    notes: str = ""


def write_rows(path: Path, rows: list[Row]) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    with path.open("w", newline="") as fh:
        w = csv.DictWriter(fh, fieldnames=list(Row.__dataclass_fields__))
        w.writeheader()
        for r in rows:
            w.writerow(asdict(r))


def write_env(path: Path, env: dict) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(env, indent=2) + "\n")


def append_manifest(path: Path, entry: dict) -> None:
    """artifact -> config -> build -> which report section cites it."""
    cols = ["artifact", "kind", "config", "build", "cites", "produced_by", "timestamp"]
    new = not path.exists()
    path.parent.mkdir(parents=True, exist_ok=True)
    with path.open("a", newline="") as fh:
        w = csv.DictWriter(fh, fieldnames=cols)
        if new:
            w.writeheader()
        w.writerow({c: entry.get(c, "") for c in cols})


def results_dir(tag: str = "") -> Path:
    """perf/results/<date>_<f32|f64>[_tag]/ — one directory per campaign."""
    ped = device_ped_type()
    short = {"float": "f32", "double": "f64"}.get(ped, ped)
    name = f"{time.strftime('%Y-%m-%d')}_{short}" + (f"_{tag}" if tag else "")
    d = Path(__file__).resolve().parent / "results" / name
    d.mkdir(parents=True, exist_ok=True)
    return d


def print_table(rows: list[Row], floor_us: float | None = None) -> None:
    """Cold (rep 0) vs warm (BEST of the rest), with the spread.

    "Best of the rest", not "last", because collect() does not converge: it
    oscillates between allocator states. Measured at 9x9, opt4, one run:
    85.8 / 73.7 / 86.6 us per frame with faults 520k / 127k / 519k. Quoting the
    last rep there reports 86.6 when 73.7 was achieved in the same run — the
    choice of rep would be doing the work, not the code.

    The spread column is therefore part of the result, not noise to hide: the
    paths that allocate per frame vary by 3-17 %, and collect_view(), which
    allocates nothing, is reproducible to a few tenths of a percent. That
    contrast is itself a finding.
    """
    by_step: dict[str, list[Row]] = {}
    for r in rows:
        by_step.setdefault(r.step, []).append(r)
    for v in by_step.values():
        v.sort(key=lambda r: r.rep)

    def warm_of(v: list[Row]) -> Row:
        return min(v[1:] or v, key=lambda r: r.us_per_frame)

    cpu = by_step.get("cpu")
    cpu_us = warm_of(cpu).us_per_frame if cpu else None

    hdr = (f"{'step':<6} {'label':<38} {'cold FPS':>9} {'warm FPS':>9} "
           f"{'warm us/f':>10} {'spread':>7} {'faults c->w':>18}")
    if cpu_us:
        hdr += f" {'vs CPU':>7}"
    if floor_us:
        hdr += f" {'%floor':>7}"
    print(hdr)
    print("-" * len(hdr))
    for step, v in by_step.items():
        cold, warm = v[0], warm_of(v)
        us = [r.us_per_frame for r in v[1:]] or [v[0].us_per_frame]
        spread = 100 * (max(us) - min(us)) / min(us)
        line = (f"{step:<6} {warm.label[:38]:<38} {cold.fps:9,.0f} {warm.fps:9,.0f} "
                f"{warm.us_per_frame:10.2f} {spread:6.1f}% "
                f"{cold.minor_faults:>8,} ->{warm.minor_faults:>8,}")
        if cpu_us:
            line += f" {cpu_us / warm.us_per_frame:6.2f}x"
        if floor_us:
            line += f" {100 * floor_us / warm.us_per_frame:6.0f}%"
        print(line)
    print("  cold = rep 0 (fresh process). warm = best of reps 1..n; spread = "
          "(max-min)/min over those reps.")
    if cpu and len(cpu) == 1:
        print("  cpu is first-pass only (stop() is terminal): cold == warm.")
