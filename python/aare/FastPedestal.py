# SPDX-License-Identifier: MPL-2.0

import numpy as np

from .factory import _get_typed_class


def _get_fast_pedestal_class(dtype):
    return _get_typed_class("FastPedestal", dtype)


def FastPedestal(rows, cols, n_samples=1000, dtype=np.float64):
    """Create an empty per-pixel running pedestal.

    This factory hides the dtype suffix used by the templated C++ bindings.
    Call ``add_init_frame()`` exactly ``n_samples`` times before using the
    statistics or calling ``push_ema()``. Subsequent frames have weight
    ``1 / n_samples`` in the running mean and population variance.

    Args:
        rows: Number of image rows.
        cols: Number of image columns.
        n_samples: Initialization frame count and steady-state update-weight
            denominator.
        dtype: Output dtype for the mean, variance, and standard deviation.
            Supported values are ``np.float64``, ``np.float32``, and
            ``np.int16``.
    """
    cls = _get_fast_pedestal_class(dtype)
    return cls(rows, cols, n_samples)


def from_file(filename, n_samples=1000, skip_first=0, dtype=np.float64):
    """Create a FastPedestal from frames in a file.

    After ignoring ``skip_first`` frames, the next ``n_samples`` frames
    initialize the pedestal. Every remaining frame is then applied as a
    steady-state update. Input frames are read as uint16 data.

    Args:
        filename: Input image file.
        n_samples: Number of frames used for initialization.
        skip_first: Number of leading frames to ignore.
        dtype: Output dtype for the mean, variance, and standard deviation.

    Raises:
        RuntimeError: If fewer than ``n_samples`` frames remain after
            ``skip_first`` or ``n_samples`` is zero.
    """
    cls = _get_fast_pedestal_class(dtype)
    return cls.from_file(
        filename, n_samples=n_samples, skip_first=skip_first
    )


FastPedestal.from_file = from_file
