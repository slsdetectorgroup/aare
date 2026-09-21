# SPDX-License-Identifier: MPL-2.0
"""Probe make_view in np_helper.hpp through the bindings in _aare.testing"""
import numpy as np
import pytest

from aare._aare import testing




def test_make_view_roundtrip():
    """Send an array to C++ and check that we get the same array back"""
    arr = np.arange(30, dtype=np.float64).reshape(5, 6)
    out = testing.view_roundtrip_2d(arr)
    np.testing.assert_array_equal(out, arr)

    #Sanity check to make sure we got a copy an not just the same thing back
    arr[3,2] = 31655
    assert arr[3,2] != out[3,2]

def test_make_view_const_roundtrip():
    """Const works if the array is not writeable"""
    arr = np.arange(30, dtype=np.float64).reshape(5, 6)
    arr.flags.writeable = False
    out = testing.const_view_roundtrip_2d(arr)
    np.testing.assert_array_equal(out, arr)

def test_make_view_const_roundtrip_writable():
    """Const works also if the array is  writeable"""
    arr = np.arange(30, dtype=np.float64).reshape(5, 6)
    out = testing.const_view_roundtrip_2d(arr)
    np.testing.assert_array_equal(out, arr)

def test_make_view_fill():
    arr = np.arange(30, dtype=np.float64).reshape(5, 6)
    testing.view_fill_2d(arr, 7)
    assert (arr == 7).all()

def test_make_view_fill_rejects_nonwriteable():
    arr = np.arange(30, dtype=np.float64).reshape(5, 6)
    arr.flags.writeable = False
    with pytest.raises(ValueError, match="not writeable"):
        testing.view_fill_2d(arr, 7)

def test_make_view_2d_rejects_1d():
    arr = np.arange(30, dtype=np.uint16)
    with pytest.raises(ValueError, match="Expected 2D array, got 1D"):
        testing.view_roundtrip_2d(arr)

def test_make_view_2d_rejects_3d():
    arr = np.arange(30, dtype=np.uint16).reshape(3, 5, 2)
    with pytest.raises(ValueError, match="Expected 2D array, got 3D"):
        testing.view_roundtrip_2d(arr)

def test_make_view_rejects_noncontiguous():
    arr = np.arange(30, dtype=np.float64).reshape(5, 6)
    arr = arr[::2, ::3]  # non-contiguous
    assert not arr.flags.c_contiguous
    with pytest.raises(ValueError, match="not C-contiguous"):
        testing.view_roundtrip_2d(arr)

def test_make_view_rejects_noncontiguous_const():
    arr = np.arange(30, dtype=np.float64).reshape(5, 6)
    arr = arr[::2, ::3]  # non-contiguous
    assert not arr.flags.c_contiguous
    with pytest.raises(ValueError, match="not C-contiguous"):
        testing.const_view_roundtrip_2d(arr)

def test_make_view_rejects_fortran_order():
    arr = np.asfortranarray(np.arange(30, dtype=np.float64).reshape(5, 6))
    assert not arr.flags.c_contiguous
    with pytest.raises(ValueError, match="not C-contiguous"):
        testing.view_roundtrip_2d(arr)

