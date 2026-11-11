"""PixelHistogramCUDA-specific settings for the probes in this folder.

Importing this module also makes ../common.py and ../gpu_span.py importable.
"""
from __future__ import annotations

import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE.parent))
import common  # noqa: E402

# Rebuild-guard: the extension must be newer than all of these.
HEADERS = ["include/aare/hist/PixelHistogramCUDA.hpp",
           "include/aare/hist/pixelhistogram_kernel.cuh",
           "include/aare/RAII_device.hpp"]

# Kernel short names in the trace, matched as substrings. The fill kernel
# (fill_window or fill_bin_major) runs once per batch; the readout kernel
# once per values() call.
FILL_KERNEL = "fill_"
READOUT_KERNEL = "bin_to_pixel_major"


def results_dir(tag: str = "") -> Path:
    """hist/results/<date>[_tag]/"""
    return common.results_dir(HERE, "", tag)
