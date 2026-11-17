"""ClusterFinderCUDA-specific settings for the probes in this folder.

Importing this module also makes ../common.py and ../gpu_span.py importable.
"""
from __future__ import annotations

import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE.parent))
import common  # noqa: E402

N_PEDESTAL_FRAMES = 1000
N_SIGMA = 5

# Rebuild-guard: the extension must be newer than all of these.
HEADERS = ["include/aare/cf/clusterfinder_kernel.cuh",
           "include/aare/cf/clusterfinder_algo.cuh",
           "include/aare/cf/ClusterFinderCUDA.hpp"]


def device_ped_type() -> str:
    """DEVICE_PED_TYPE from the kernel header (the float/double build axis)."""
    hdr = common.REPO / "include/aare/cf/clusterfinder_kernel.cuh"
    for line in hdr.read_text().splitlines()[:40]:
        if "using DEVICE_PED_TYPE" in line and not line.strip().startswith("//"):
            return line.split("=")[1].split(";")[0].strip()
    return "unknown"


def results_dir(tag: str = "") -> Path:
    """cf/results/<date>_<f32|f64>[_tag]/"""
    short = {"float": "f32", "double": "f64"}.get(device_ped_type(), "unk")
    return common.results_dir(HERE, short, tag)
