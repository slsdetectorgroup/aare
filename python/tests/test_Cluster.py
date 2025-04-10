import pytest 
import numpy as np

import aare._aare as aare
from conftest import test_data_path


def test_ClusterVector(): 
    """Test ClusterVector""" 

    clustervector = aare.ClusterVector_Cluster3x3i()
    assert clustervector.cluster_size_x == 3
    assert clustervector.cluster_size_y == 3
    assert clustervector.item_size() == 4+9*4
    assert clustervector.frame_number == 0
    assert clustervector.capacity == 1024
    assert clustervector.size == 0

    cluster = aare.Cluster3x3i(0,0,np.ones(9, dtype=np.int32))
    
    clustervector.push_back(cluster) 
    assert clustervector.size == 1

    with pytest.raises(TypeError):  # Or use the appropriate exception type
        clustervector.push_back(aare.Cluster2x2i(0,0,np.ones(4, dtype=np.int32)))

    with pytest.raises(TypeError): 
        clustervector.push_back(aare.Cluster3x3f(0,0,np.ones(9, dtype=np.float32))) 

def test_Interpolator(): 
    """Test Interpolator"""

    ebins = np.linspace(0,10, 20, dtype=np.float64)
    xbins = np.linspace(0, 5, 30, dtype=np.float64)
    ybins = np.linspace(0, 5, 30, dtype=np.float64)

    etacube = np.zeros(shape=[30, 30, 20], dtype=np.float64)
    interpolator = aare.Interpolator(etacube, xbins, ybins, ebins)

    assert interpolator.get_ietax().shape == (30,30,20)
    assert interpolator.get_ietay().shape == (30,30,20)
    clustervector = aare.ClusterVector_Cluster3x3i()
    
    cluster = aare.Cluster3x3i(0,0, np.ones(9, dtype=np.int32))
    clustervector.push_back(cluster) 

    interpolated_photons = interpolator.interpolate(clustervector)

    assert interpolated_photons.size == 1

    assert interpolated_photons[0]["x"] == -1 
    assert interpolated_photons[0]["y"] == -1
    assert interpolated_photons[0]["energy"] == 4 #eta_sum = 4, dx, dy = -1,-1 m_ietax = 0, m_ietay = 0

    clustervector = aare.ClusterVector_Cluster2x2i()

    cluster = aare.Cluster2x2i(0,0, np.ones(4, dtype=np.int32))
    clustervector.push_back(cluster) 

    interpolated_photons = interpolator.interpolate(clustervector)

    assert interpolated_photons.size == 1

    assert interpolated_photons[0]["x"] == 0
    assert interpolated_photons[0]["y"] == 0
    assert interpolated_photons[0]["energy"] == 4

@pytest.mark.files
def test_cluster_file(test_data_path): 
    """Test ClusterFile""" 
    cluster_file = aare.ClusterFile_Cluster3x3i(test_data_path / "clust/single_frame_97_clustrers.clust") 
    clustervector = cluster_file.read_clusters(10) #conversion does not work

    cluster_file.close()

    assert clustervector.size == 10

    ###reading with wrong file
    with pytest.raises(TypeError): 
        cluster_file = aare.ClusterFile_Cluster2x2i(test_data_path / "clust/single_frame_97_clustrers.clust") 
        cluster_file.close()

def test_calculate_eta(): 
    """Calculate Eta""" 
    clusters = aare.ClusterVector_Cluster3x3i() 
    clusters.push_back(aare.Cluster3x3i(0,0, np.ones(9, dtype=np.int32)))
    clusters.push_back(aare.Cluster3x3i(0,0, np.array([1,1,1,2,2,2,3,3,3])))

    eta2 = aare.calculate_eta2(clusters)

    assert eta2.shape == (2,2)
    assert eta2[0,0] == 0.5
    assert eta2[0,1] == 0.5
    assert eta2[1,0] == 0.5
    assert eta2[1,1] == 0.6 #1/5

def test_cluster_finder(): 
    """Test ClusterFinder""" 

    clusterfinder = aare.ClusterFinder_Cluster3x3i([100,100])

    #frame = np.random.rand(100,100)
    frame = np.zeros(shape=[100,100])

    clusterfinder.find_clusters(frame)

    clusters = clusterfinder.steal_clusters(False) #conversion does not work

    assert clusters.size == 0


#TODO dont understand behavior
def test_cluster_collector(): 
    """Test ClusterCollector"""

    clusterfinder = aare.ClusterFinderMT_Cluster3x3i([100,100]) #TODO: no idea what the data is in InputQueue not zero 

    clustercollector = aare.ClusterCollector_Cluster3x3i(clusterfinder)

    cluster_vectors = clustercollector.steal_clusters()

    assert len(cluster_vectors) == 1 #single thread execution
    assert cluster_vectors[0].size == 0 #






