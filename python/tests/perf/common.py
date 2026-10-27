"""Shared plumbing for the nsys probes: paths, guards, environment stamp.

Used by every probe folder (cf/, ...). Settings specific to one class, such as
the headers to check or extra env.json fields, are passed in by that folder.

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

NSYS = os.environ.get("NSYS", "/opt/nvidia/nsight-systems/2024.5.1/bin/nsys")


def _sh(cmd: str, default: str = "unknown") -> str:
    try:
        return subprocess.run(cmd, shell=True, capture_output=True, text=True,
                              timeout=30).stdout.strip() or default
    except Exception:
        return default


def assert_build_fresh(headers: list[str]) -> None:
    """Abort if any header (repo-relative path) is newer than the extension."""
    so = list((BUILD / "aare").glob("_aare_cuda*.so"))
    if not so:
        raise RuntimeError(f"no _aare_cuda*.so under {BUILD / 'aare'}")
    built = max(f.stat().st_mtime for f in so)
    stale = [h for h in headers if (REPO / h).stat().st_mtime > built]
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


def capture_env(**extra) -> dict:
    """Machine, build and git state; `extra` adds probe-specific fields."""
    import aare

    env = {
        "timestamp": time.strftime("%Y-%m-%d %H:%M:%S"),
        "host": _sh("hostname"),
        "git_rev": _sh(f"git -C {REPO} rev-parse --short HEAD"),
        "git_branch": _sh(f"git -C {REPO} rev-parse --abbrev-ref HEAD"),
        "git_dirty": bool(_sh(f"git -C {REPO} status --porcelain", default="")),
        "aare_version": getattr(aare, "__version__", "unknown"),
    }
    env.update(extra)
    env.update({
        "gpu": _sh("nvidia-smi --query-gpu=name --format=csv,noheader"),
        "driver": _sh("nvidia-smi --query-gpu=driver_version --format=csv,noheader"),
        "nvcc": _sh("nvcc --version | tail -1"),
        "python": sys.version.split()[0],
    })
    return env


def results_dir(probe_dir: Path, suffix: str = "", tag: str = "") -> Path:
    """<probe_dir>/results/<date>[_suffix][_tag]/, created if missing."""
    name = time.strftime("%Y-%m-%d") + "".join(f"_{s}" for s in (suffix, tag) if s)
    d = probe_dir / "results" / name
    d.mkdir(parents=True, exist_ok=True)
    return d


def write_env(path: Path, env: dict) -> None:
    path.write_text(json.dumps(env, indent=2) + "\n")
