# SPDX-License-Identifier: MPL-2.0

import numpy as np

from . import _aare


_TYPE_TO_CHAR = {
    np.dtype(np.int32): "i",
    np.dtype(np.float32): "f",
    np.dtype(np.float64): "d",
    np.dtype(np.int16): "i16",
}


def _type_to_char(dtype):
    """Return the suffix used by bindings instantiated for ``dtype``."""
    try:
        return _TYPE_TO_CHAR[np.dtype(dtype)]
    except (KeyError, TypeError):
        supported = ", ".join(str(dtype) for dtype in _TYPE_TO_CHAR)
        raise ValueError(
            f"Unsupported dtype: {dtype}. Supported dtypes are {supported}."
        ) from None


def _get_typed_class(name, dtype):
    """Return a bound class named ``<name>_<dtype suffix>``."""
    class_name = f"{name}_{_type_to_char(dtype)}"
    try:
        return getattr(_aare, class_name)
    except AttributeError:
        raise ValueError(
            f"Unsupported dtype for {name}: {dtype} "
            f"(binding {class_name} is not available)."
        ) from None
