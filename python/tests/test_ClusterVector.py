import pytest 
import numpy as np
import boost_histogram as bh
import time
from pathlib import Path
import pickle

from aare import ClusterFile
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
    