# SPDX-License-Identifier: MPL-2.0
"""Probe make_view in np_helper.hpp through the bindings in _aare.testing"""
import numpy as np
import pytest

from aare._aare import testing

SHAPES = {1: (7,), 2: (5, 6), 3: (3, 5, 6)}
DTYPES = [np.float64, np.uint16]


def _probe(name, ndim):
    return getattr(testing, f"{name}_{ndim}d")


def _array(ndim, dtype):
    shape = SHAPES[ndim]
    return np.arange(np.prod(shape), dtype=dtype).reshape(shape)


def _c_strides(shape):
    # strides in elements, as used by NDView
    return tuple(int(np.prod(shape[i + 1 :])) for i in range(len(shape)))


def _noncontiguous(ndim, dtype, layout):
    arr = _array(ndim, dtype)
    if layout == "sliced":
        out = np.repeat(arr, 2, axis=-1)[..., ::2]
    elif layout == "reversed":
        out = np.ascontiguousarray(arr[..., ::-1])[..., ::-1]
    elif layout == "fortran":
        out = np.asfortranarray(arr)
    elif layout == "transposed":
        out = np.ascontiguousarray(arr.T).T
    np.testing.assert_array_equal(out, arr)
    assert not out.flags.c_contiguous
    return out


VIEWS = ["view", "const_view"]


@pytest.mark.parametrize("view", VIEWS)
@pytest.mark.parametrize("dtype", DTYPES)
@pytest.mark.parametrize("ndim", [1, 2, 3])
def test_view_of_contiguous_array(ndim, dtype, view):
    arr = _array(ndim, dtype)

    out = _probe(f"{view}_roundtrip", ndim)(arr)
    assert out.dtype == dtype
    np.testing.assert_array_equal(out, arr)

    shape, strides, ptr = _probe(f"{view}_info", ndim)(arr)
    assert tuple(shape) == arr.shape
    assert tuple(strides) == _c_strides(arr.shape)
    assert ptr == arr.ctypes.data  # no copy


@pytest.mark.parametrize("dtype", DTYPES)
@pytest.mark.parametrize("ndim", [1, 2, 3])
def test_write_through_view_reaches_caller(ndim, dtype):
    arr = _array(ndim, dtype)
    _probe("view_fill", ndim)(arr, 7)
    assert (arr == 7).all()


@pytest.mark.parametrize("dtype", DTYPES)
@pytest.mark.parametrize(
    "ndim, layout",
    [
        (1, "sliced"),
        (1, "reversed"),
        (2, "sliced"),
        (2, "reversed"),
        (2, "fortran"),
        (2, "transposed"),
        (3, "sliced"),
        (3, "fortran"),
        (3, "transposed"),
    ],
)
def test_noncontiguous_array_is_rejected(ndim, layout, dtype):
    arr = _noncontiguous(ndim, dtype, layout)
    for name in [
        "view_roundtrip",
        "view_info",
        "const_view_roundtrip",
        "const_view_info",
    ]:
        with pytest.raises(ValueError, match="not C-contiguous"):
            _probe(name, ndim)(arr)

    # and nothing is written to it
    before = arr.copy()
    with pytest.raises(ValueError, match="not C-contiguous"):
        _probe("view_fill", ndim)(arr, 7)
    np.testing.assert_array_equal(arr, before)

    # making it contiguous is the way out
    out = _probe("view_roundtrip", ndim)(np.ascontiguousarray(arr))
    np.testing.assert_array_equal(out, arr)


@pytest.mark.parametrize("view", VIEWS)
@pytest.mark.parametrize("dtype", DTYPES)
@pytest.mark.parametrize(
    "expected, got", [(1, 2), (1, 3), (2, 1), (2, 3), (3, 1), (3, 2)]
)
def test_wrong_number_of_dimensions_is_rejected(expected, got, dtype, view):
    arr = _array(got, dtype)
    with pytest.raises(ValueError, match=f"Expected {expected}D array, got {got}D"):
        _probe(f"{view}_roundtrip", expected)(arr)


def test_zero_dimensional_array_is_rejected():
    with pytest.raises(ValueError, match="Expected 1D array, got 0D"):
        testing.view_roundtrip_1d(np.array(1.0))


def test_dimensions_are_checked_before_layout():
    arr = _noncontiguous(3, np.float64, "fortran")
    with pytest.raises(ValueError, match="Expected 2D array, got 3D"):
        testing.view_roundtrip_2d(arr)


@pytest.mark.parametrize("view", VIEWS)
@pytest.mark.parametrize(
    "shape", [(0,), (1,), (0, 4), (4, 0), (1, 4), (4, 1), (1, 1), (1, 4, 1), (2, 0, 3)]
)
def test_empty_and_unit_length_dimensions(shape, view):
    arr = np.arange(np.prod(shape), dtype=np.float64).reshape(shape)
    out = _probe(f"{view}_roundtrip", len(shape))(arr)
    assert out.shape == shape
    np.testing.assert_array_equal(out, arr)


def test_unit_length_dimensions_that_numpy_flags_as_contiguous():
    # Fortran order and slicing a length one axis still give a C-contiguous array
    for arr in [
        np.asfortranarray(np.arange(4.0).reshape(1, 4)),
        np.asfortranarray(np.arange(4.0).reshape(4, 1)),
        np.arange(8.0).reshape(2, 4)[:1],
    ]:
        assert arr.flags.c_contiguous
        np.testing.assert_array_equal(testing.view_roundtrip_2d(arr), arr)


def test_contiguous_slices_are_viewed_in_place():
    base = np.arange(60, dtype=np.float64).reshape(10, 6)
    rows = base[2:5]
    assert rows.flags.c_contiguous and not rows.flags.owndata

    shape, _, ptr = testing.view_info_2d(rows)
    assert tuple(shape) == (3, 6)
    assert ptr == rows.ctypes.data
    np.testing.assert_array_equal(testing.view_roundtrip_2d(rows), rows)

    testing.view_fill_2d(rows, -1)
    assert (base[2:5] == -1).all()
    assert (base[:2] != -1).all() and (base[5:] != -1).all()


def test_read_only_array_is_rejected_by_mutable_view():
    arr = _array(2, np.float64)
    arr.flags.writeable = False
    with pytest.raises(ValueError, match="not writeable"):
        testing.view_roundtrip_2d(arr)


@pytest.mark.parametrize("dtype", DTYPES)
@pytest.mark.parametrize("ndim", [1, 2, 3])
def test_read_only_array_is_accepted_by_const_view(ndim, dtype):
    arr = _array(ndim, dtype)
    arr.flags.writeable = False

    out = _probe("const_view_roundtrip", ndim)(arr)
    np.testing.assert_array_equal(out, arr)

    _, _, ptr = _probe("const_view_info", ndim)(arr)
    assert ptr == arr.ctypes.data  # no copy


@pytest.mark.parametrize("view", VIEWS)
@pytest.mark.parametrize("ndim", [1, 2, 3])
def test_make_view_nd_wrappers(ndim, view):
    wrapper = getattr(testing, f"make_{view}_{ndim}d_info")
    arr = _array(ndim, np.float64)
    shape, strides = wrapper(arr)
    assert tuple(shape) == arr.shape
    assert tuple(strides) == _c_strides(arr.shape)

    wrong = _array(ndim % 3 + 1, np.float64)
    with pytest.raises(ValueError, match=f"Expected {ndim}D array, got {wrong.ndim}D"):
        wrapper(wrong)

    with pytest.raises(ValueError, match="not C-contiguous"):
        wrapper(_noncontiguous(ndim, np.float64, "sliced"))


def test_probes_dispatch_on_dtype_without_converting():
    with pytest.raises(TypeError):
        testing.view_roundtrip_2d(_array(2, np.float32))
    with pytest.raises(TypeError):
        testing.view_roundtrip_2d(_array(2, np.float64).tolist())


@pytest.mark.parametrize("layout", ["sliced", "reversed", "fortran", "transposed"])
def test_c_style_signature_converts_before_make_view(layout):
    # When the binding asks pybind11 for c_style the check in make_view never
    # fires, the view is of a temporary and writes are lost
    arr = _noncontiguous(2, np.float64, layout)
    np.testing.assert_array_equal(testing.view_roundtrip_2d_c_style(arr), arr)

    before = arr.copy()
    testing.view_fill_2d_c_style(arr, 7)
    np.testing.assert_array_equal(arr, before)

    contiguous = _array(2, np.float64)
    testing.view_fill_2d_c_style(contiguous, 7)
    assert (contiguous == 7).all()


def test_c_style_signature_still_checks_dimensions():
    with pytest.raises(ValueError, match="Expected 2D array, got 3D"):
        testing.view_roundtrip_2d_c_style(_array(3, np.float64))
