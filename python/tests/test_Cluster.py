import pytest 
import numpy as np

from aare import _aare #import the C++ module
from conftest import test_data_path


def test_cluster_vector_can_be_converted_to_numpy():
    cv = _aare.ClusterVector_Cluster3x3i()
    arr = np.array(cv, copy=False)
    assert arr.shape == (0,)  # 4 for x, y, size, energy and 9 for the cluster data


def test_ClusterVector(): 
    """Test ClusterVector""" 

    clustervector = _aare.ClusterVector_Cluster3x3i()
    assert clustervector.cluster_size_x == 3
    assert clustervector.cluster_size_y == 3
    assert clustervector.item_size() == 4+9*4
    assert clustervector.frame_number == 0
    assert clustervector.size == 0

    cluster = _aare.Cluster3x3i(0,0,np.ones(9, dtype=np.int32))
    
    clustervector.push_back(cluster) 
    assert clustervector.size == 1

    with pytest.raises(TypeError):  # Or use the appropriate exception type
        clustervector.push_back(_aare.Cluster2x2i(0,0,np.ones(4, dtype=np.int32)))

    with pytest.raises(TypeError): 
        clustervector.push_back(_aare.Cluster3x3f(0,0,np.ones(9, dtype=np.float32))) 

def test_Interpolator(): 
    """Test Interpolator"""

    ebins = np.linspace(0,10, 20, dtype=np.float64)
    xbins = np.linspace(0, 5, 30, dtype=np.float64)
    ybins = np.linspace(0, 5, 30, dtype=np.float64)

    etacube = np.zeros(shape=[30, 30, 20], dtype=np.float64)
    interpolator = _aare.Interpolator(etacube, xbins, ybins, ebins)

    assert interpolator.get_ietax().shape == (30,30,20)
    assert interpolator.get_ietay().shape == (30,30,20)
    clustervector = _aare.ClusterVector_Cluster3x3i()
    
    cluster = _aare.Cluster3x3i(0,0, np.ones(9, dtype=np.int32))
    clustervector.push_back(cluster) 

    interpolated_photons = interpolator.interpolate(clustervector)

    assert interpolated_photons.size == 1

    assert interpolated_photons[0]["x"] == -1 
    assert interpolated_photons[0]["y"] == -1
    assert interpolated_photons[0]["energy"] == 4 #eta_sum = 4, dx, dy = -1,-1 m_ietax = 0, m_ietay = 0

    clustervector = _aare.ClusterVector_Cluster2x2i()

    cluster = _aare.Cluster2x2i(0,0, np.ones(4, dtype=np.int32))
    clustervector.push_back(cluster) 

    interpolated_photons = interpolator.interpolate(clustervector)

    assert interpolated_photons.size == 1

    assert interpolated_photons[0]["x"] == 0
    assert interpolated_photons[0]["y"] == 0
    assert interpolated_photons[0]["energy"] == 4



def test_calculate_eta(): 
    """Calculate Eta""" 
    clusters = _aare.ClusterVector_Cluster3x3i() 
    clusters.push_back(_aare.Cluster3x3i(0,0, np.ones(9, dtype=np.int32)))
    clusters.push_back(_aare.Cluster3x3i(0,0, np.array([1,1,1,2,2,2,3,3,3])))

    eta2 = _aare.calculate_eta2(clusters)

    assert eta2.shape == (2,2)
    assert eta2[0,0] == 0.5
    assert eta2[0,1] == 0.5
    assert eta2[1,0] == 0.5
    assert eta2[1,1] == 0.6 #1/5

def test_cluster_finder(): 
    """Test ClusterFinder""" 

    clusterfinder = _aare.ClusterFinder_Cluster3x3i([100,100])

    #frame = np.random.rand(100,100)
    frame = np.zeros(shape=[100,100])

    clusterfinder.find_clusters(frame)

    clusters = clusterfinder.steal_clusters(False) #conversion does not work

    assert clusters.size == 0









