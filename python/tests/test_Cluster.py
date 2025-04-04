import pytest 
import numpy as np

from _aare import ClusterVector_Cluster3x3i, Interpolator, Cluster3x3i, ClusterFinder_Cluster3x3i

def test_ClusterVector(): 
    """Test ClusterVector""" 

    clustervector = ClusterVector_Cluster3x3i()
    assert clustervector.cluster_size_x == 3
    assert clustervector.cluster_size_y == 3
    assert clustervector.item_size() == 4+9*4
    assert clustervector.frame_number == 0
    assert clustervector.capacity == 1024
    assert clustervector.size == 0

    cluster = Cluster3x3i(0,0,np.ones(9, dtype=np.int32))
    
    #clustervector.push_back(cluster) 
    #assert clustervector.size == 1

    #push_back - check size


def test_Interpolator(): 
    """Test Interpolator"""

    ebins = np.linspace(0,10, 20, dtype=np.float64)
    xbins = np.linspace(0, 5, 30, dtype=np.float64)
    ybins = np.linspace(0, 5, 30, dtype=np.float64)

    etacube = np.zeros(shape=[30, 30, 20], dtype=np.float64)
    interpolator = Interpolator(etacube, xbins, ybins, ebins)

    assert interpolator.get_ietax().shape == (30,30,20)
    assert interpolator.get_ietay().shape == (30,30,20)
    clustervector = ClusterVector_Cluster3x3i()
    
    #TODO clustervector is empty 
    cluster = Cluster3x3i(0,0, np.ones(9, dtype=np.int32))
    #clustervector.push_back(cluster) 
    num_clusters = 1; 

    assert interpolator.interpolate_Cluster3x3i(clustervector).shape == (num_clusters, 3)


#def test_cluster_file(): 

#def test_cluster_finder(): 
    #"""Test ClusterFinder""" 

    #clusterfinder = ClusterFinder_Cluster3x3i([100,100])

    #clusterfinder.find_clusters()

    #clusters = clusterfinder.steal_clusters()

    #print("cluster size: ", clusters.size()) 






