# SPDX-License-Identifier: MPL-2.0

import numpy as np

from .factory import _get_typed_class, _stored_state_fields


def Pedestal(rows, cols, n_samples=1000, dtype=np.float64):
    """Create an empty per-pixel running pedestal.

    This factory hides the dtype suffix used by the templated C++ bindings.
    Call ``push()`` to update the statistics and cached mean for each frame.
    Statistics are available during initialization and are zero for empty pixels.
    Internal moments and variance always use double precision.

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


def load_state(filename, dtype=None):
    """Create a Pedestal from a state file written by ``save_state()``.

    The new pedestal continues where the stored one stopped. To load into a
    pedestal that already exists, call ``load_state(filename)`` on it.

    Args:
        filename: State file written by ``save_state()``.
        dtype: Output dtype for the mean and standard deviation. By default
            the dtype of the pedestal that wrote the file, as stored in it.

    Raises:
        RuntimeError: If the file cannot be read, is not a state file of a
            Pedestal, or is truncated or damaged.
        ValueError: If the dtype, given or stored, has no binding.
    """
    if dtype is None:
        (dtype,) = _stored_state_fields(filename, "Pedestal", "dtype")
    cls = _get_typed_class("Pedestal", dtype)
    return cls.from_state_file(filename)


Pedestal.load_state = load_state
