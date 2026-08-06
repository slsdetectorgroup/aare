# SPDX-License-Identifier: MPL-2.0

import numpy as np

from .factory import _get_typed_class


def _get_fast_pedestal_class(dtype):
    return _get_typed_class("FastPedestal", dtype)


def FastPedestal(rows, cols, n_samples=1000, dtype=np.float64):
    """Create a FastPedestal with the requested output dtype.

    This factory hides the dtype suffix used by the templated C++ bindings.
    Supported dtypes are ``np.float64``, ``np.float32``, and ``np.int16``.
    """
    cls = _get_fast_pedestal_class(dtype)
    return cls(rows, cols, n_samples)


def from_file(filename, n_samples=1000, skip_first=0, dtype=np.float64):
    """Create a FastPedestal from frames in a file.

    The input frames must contain uint16 data. ``dtype`` selects the output
    type of the pedestal mean, variance, and standard deviation.
    """
    cls = _get_fast_pedestal_class(dtype)
    return cls.from_file(
        filename, n_samples=n_samples, skip_first=skip_first
    )


FastPedestal.from_file = from_file
