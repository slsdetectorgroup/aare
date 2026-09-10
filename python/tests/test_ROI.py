import pytest 

import numpy as np

from aare import ROI 


def test_roi_slice():
    """test the ROI slice method"""
    roi = ROI(0, 4, 2, 4)

    array = np.array([[1, 2, 3, 4],
                      [5, 6, 7, 8],
                      [9, 10, 11, 12],
                      [13, 14, 15, 16]])
    
    sliced_array = array[roi.slice()]

    assert sliced_array.shape == (2, 4)
    assert np.array_equal(sliced_array, np.array([[9, 10, 11, 12],
                                                   [13, 14, 15, 16]]))