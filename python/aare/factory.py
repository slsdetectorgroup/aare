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


def _stored_state_fields(filename, type_name, *keys):
    """Return fields of the state in the header of a state file.

    Used by the factories that pick a bound class from what a file records
    before loading it. A key is the name of a field or, for a field deeper
    in the state, a tuple of names and list indices. Raises ``RuntimeError``
    if the file is not a state file of ``type_name`` or lacks one of the
    fields.
    """
    header = _aare.read_state_header(filename)
    stored_type = header.get("type")
    if stored_type != type_name:
        raise RuntimeError(
            f"State file {filename}: holds the state of a {stored_type}, "
            f"expected a {type_name}"
        )
    def lookup(key):
        # A tuple is a path into the state, as ("finders", 0, "n_sigma")
        value = header["state"]
        for part in key if isinstance(key, tuple) else (key,):
            value = value[part]
        return value

    try:
        return [lookup(key) for key in keys]
    except (KeyError, IndexError, TypeError) as e:
        raise RuntimeError(
            f"State file {filename}: {e} is missing from the state"
        ) from None
