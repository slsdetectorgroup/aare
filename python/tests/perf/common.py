"""Shared plumbing for the nsys probes: paths, guards, environment stamp.

Two guards are enforced here because both have produced wrong numbers before:
a stale extension (header edited after the build) and a busy GPU (another
process leaves per-op averages intact but destroys duty cycles).
"""
from __future__ import annotations

import json
import os
import subprocess
import sys
import time
from pathlib import Path

HERE = Path(__file__).resolve().parent
REPO = Path(os.environ.get("AARE_REPO", HERE.parents[2]))
BUILD = Path(os.environ.get("AARE_BUILD", REPO / "build"))
sys.path.insert(0, str(BUILD))

DATA_DIR = Path(os.environ.get(
    "AARE_PERF_DATA",
    "/mnt/sls_det_storage/moench_data/2603_MaxIVBeamtime/2026032408/process/xrf/"))
DATA_FILE = DATA_DIR / "Cu_factor_10_data_master_0.json"
PEDESTAL_FILE = DATA_DIR / "Cu_factor_10_pedestal_master_0.json"

N_PEDESTAL_FRAMES = 1000
N_SIGMA = 5

NSYS = os.environ.get("NSYS", "/opt/nvidia/nsight-systems/2024.5.1/bin/nsys")


def _sh(cmd: str, default: str = "unknown") -> str:
    try:
        return subprocess.run(cmd, shell=True, capture_output=True, text=True,
                              timeout=30).stdout.strip() or default
    except Exception:
        return default


def device_ped_type() -> str:
    """DEVICE_PED_TYPE from the kernel header (the float/double build axis)."""
    hdr = REPO / "include/aare/clusterfinder_kernel.cuh"
    for line in hdr.read_text().splitlines()[:40]:
        if "using DEVICE_PED_TYPE" in line and not line.strip().startswith("//"):
            return line.split("=")[1].split(";")[0].strip()
    return "unknown"


def assert_build_fresh() -> None:
    """Abort if a CUDA header is newer than the installed extension."""
    so = list((BUILD / "aare").glob("_aare_cuda*.so"))
    if not so:
        raise RuntimeError(f"no _aare_cuda*.so under {BUILD / 'aare'}")
    built = max(f.stat().st_mtime for f in so)
    headers = ["clusterfinder_kernel.cuh", "clusterfinder_algo.cuh",
               "ClusterFinderCUDA.hpp"]
    stale = [h for h in headers
             if (REPO / "include/aare" / h).stat().st_mtime > built]
    if stale:
        raise RuntimeError(f"{', '.join(stale)} newer than the extension: rebuild first")


def assert_idle_gpu(max_pct: int = 5) -> None:
    pct = _sh("nvidia-smi --query-gpu=utilization.gpu --format=csv,noheader")
    try:
        val = int(pct.split()[0])
    except Exception:
        return
    if val > max_pct:
        raise SystemExit(f"GPU is {val}% busy: numbers would not be quotable. Aborting.")


def capture_env() -> dict:
    import aare

    return {
        "timestamp": time.strftime("%Y-%m-%d %H:%M:%S"),
        "host": _sh("hostname"),
        "git_rev": _sh(f"git -C {REPO} rev-parse --short HEAD"),
        "git_branch": _sh(f"git -C {REPO} rev-parse --abbrev-ref HEAD"),
        "git_dirty": bool(_sh(f"git -C {REPO} status --porcelain", default="")),
        "aare_version": getattr(aare, "__version__", "unknown"),
        "device_ped_type": device_ped_type(),
        "gpu": _sh("nvidia-smi --query-gpu=name --format=csv,noheader"),
        "driver": _sh("nvidia-smi --query-gpu=driver_version --format=csv,noheader"),
        "nvcc": _sh("nvcc --version | tail -1"),
        "python": sys.version.split()[0],
    }


def results_dir(tag: str = "") -> Path:
    """perf/results/<date>_<f32|f64>[_tag]/"""
    short = {"float": "f32", "double": "f64"}.get(device_ped_type(), "unk")
    name = f"{time.strftime('%Y-%m-%d')}_{short}" + (f"_{tag}" if tag else "")
    d = HERE / "results" / name
    d.mkdir(parents=True, exist_ok=True)
    return d


def write_env(path: Path, env: dict) -> None:
    path.write_text(json.dumps(env, indent=2) + "\n")
