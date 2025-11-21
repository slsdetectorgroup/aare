# SPDX-License-Identifier: MPL-2.0
import pytest 
import numpy as np
import boost_histogram as bh
import time
from pathlib import Path
import pickle

from aare import ClusterFile, ClusterVector, calculate_eta2
from aare import _aare
from conftest import test_data_path


def test_create_cluster_vector():
    cv = _aare.ClusterVector_Cluster3x3i()
    assert cv.cluster_size_x == 3
    assert cv.cluster_size_y == 3
    assert cv.size == 0


def test_push_back_on_cluster_vector():
    cv = _aare.ClusterVector_Cluster2x2i()
    assert cv.cluster_size_x == 2
    assert cv.cluster_size_y == 2
    assert cv.size == 0

    cluster = _aare.Cluster2x2i(19, 22, np.ones(4, dtype=np.int32))
    cv.push_back(cluster)
    assert cv.size == 1

    arr = np.array(cv, copy=False)
    assert arr[0]['x'] == 19
    assert arr[0]['y'] == 22


def test_max_2x2_sum(): 
    """max_2x2_sum"""
    cv = _aare.ClusterVector_Cluster3x3i()
    cv.push_back(_aare.Cluster3x3i(19, 22, np.array([0,1,0,2,3,0,2,1,0], dtype=np.int32)))
    cv.push_back(_aare.Cluster3x3i(19, 22, np.ones(9, dtype=np.int32)))
    assert cv.size == 2
    max_2x2 = cv.sum_2x2()
    assert max_2x2.size == 2
    assert max_2x2[0]["sum"] == 8
    assert max_2x2[0]["index"] == 2


def test_eta2(): 
    """calculate eta2"""
    cv = _aare.ClusterVector_Cluster3x3i()
    cv.push_back(_aare.Cluster3x3i(19, 22, np.ones(9, dtype=np.int32)))
    assert cv.size == 1
    eta2 = calculate_eta2(cv)
    assert eta2.size == 1
    assert eta2[0]["x"] == 0.5
    assert eta2[0]["y"] == 0.5
    assert eta2[0]["c"] == 0
    assert eta2[0]["sum"] == 4


def test_make_a_hitmap_from_cluster_vector():
    cv = _aare.ClusterVector_Cluster3x3i()

    # Push back 4 clusters with different positions
    cv.push_back(_aare.Cluster3x3i(0, 0, np.ones(9, dtype=np.int32)))
    cv.push_back(_aare.Cluster3x3i(1, 1, np.ones(9, dtype=np.int32)))
    cv.push_back(_aare.Cluster3x3i(1, 1, np.ones(9, dtype=np.int32)))
    cv.push_back(_aare.Cluster3x3i(2, 2, np.ones(9, dtype=np.int32)))

    ref = np.zeros((5, 5), dtype=np.int32)
    ref[0,0] = 1
    ref[1,1] = 2
    ref[2,2] = 1


    img = _aare.hitmap((5,5), cv)
    # print(img)
    # print(ref)
    assert (img == ref).all()


def test_2x2_reduction(): 
    cv = ClusterVector((3,3))

    cv.push_back(_aare.Cluster3x3i(5, 5, np.array([1, 1, 1, 2, 3, 1, 2, 2, 1], dtype=np.int32)))
    cv.push_back(_aare.Cluster3x3i(5, 5, np.array([2, 2, 1, 2, 3, 1, 1, 1, 1], dtype=np.int32)))

    reduced_cv = np.array(_aare.reduce_to_2x2(cv), copy=False) 

    assert reduced_cv.size == 2
    assert reduced_cv[0]["x"] == 5
    assert reduced_cv[0]["y"] == 5
    assert (reduced_cv[0]["data"] == np.array([[2, 3], [2, 2]], dtype=np.int32)).all()
    assert reduced_cv[1]["x"] == 5
    assert reduced_cv[1]["y"] == 5
    assert (reduced_cv[1]["data"] == np.array([[2, 2], [2, 3]], dtype=np.int32)).all()
    
    
def test_3x3_reduction(): 
    cv = _aare.ClusterVector_Cluster5x5d()
    
    cv.push_back(_aare.Cluster5x5d(5,5,np.array([1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 2.0, 1.0, 1.0, 1.0, 2.0, 2.0, 3.0,
                                   1.0, 1.0, 1.0, 2.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0], dtype=np.double)))
    cv.push_back(_aare.Cluster5x5d(5,5,np.array([1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 2.0, 1.0, 1.0, 1.0, 2.0, 2.0, 3.0,
                                   1.0, 1.0, 1.0, 2.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0], dtype=np.double)))
    
    reduced_cv = np.array(_aare.reduce_to_3x3(cv), copy=False)  

    assert reduced_cv.size == 2
    assert reduced_cv[0]["x"] == 5
    assert reduced_cv[0]["y"] == 5
    assert (reduced_cv[0]["data"] == np.array([[2.0, 1.0, 1.0], [2.0, 3.0, 1.0], [2.0, 1.0, 1.0]], dtype=np.double)).all()