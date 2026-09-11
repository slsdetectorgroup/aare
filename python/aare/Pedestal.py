# SPDX-License-Identifier: MPL-2.0

import numpy as np

from .factory import _get_typed_class


def Pedestal(rows, cols, n_samples=1000, dtype=np.float64):
    """Create an empty per-pixel running pedestal.

    This factory hides the dtype suffix used by the templated C++ bindings.
    Call ``push()`` to update the statistics and cached mean for each frame.
    Internal sums and sums of squares always use double precision.

    Args:
        rows: Number of image rows.
        cols: Number of image columns.
        n_samples: Number of samples accumulated before switching to
            steady-state updates with weight ``1 / n_samples``.
        dtype: Output dtype for the mean and standard deviation.
            Supported values are ``np.float64``, ``np.float32``, and
            ``np.int16``.
    """
    cls = _get_typed_class("Pedestal", dtype)
    return cls(rows, cols, n_samples)
