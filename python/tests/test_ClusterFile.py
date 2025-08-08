
import pytest 
import numpy as np
import boost_histogram as bh
import time
from pathlib import Path
import pickle

from aare import ClusterFile
from conftest import test_data_path

@pytest.mark.withdata
def test_cluster_file(test_data_path): 
    """Test ClusterFile""" 
    f =  ClusterFile(test_data_path / "clust/single_frame_97_clustrers.clust") 
    cv = f.read_clusters(10) #conversion does not work


    assert cv.frame_number == 135
    assert cv.size == 10

    #Known data 
    #frame_number, num_clusters   [135] 97
    #[  1 200] [0 1 2 3 4 5 6 7 8]
    #[  2 201] [ 9 10 11 12 13 14 15 16 17]
    #[  3 202] [18 19 20 21 22 23 24 25 26]
    #[  4 203] [27 28 29 30 31 32 33 34 35]
    #[  5 204] [36 37 38 39 40 41 42 43 44]
    #[  6 205] [45 46 47 48 49 50 51 52 53]
    #[  7 206] [54 55 56 57 58 59 60 61 62]
    #[  8 207] [63 64 65 66 67 68 69 70 71]
    #[  9 208] [72 73 74 75 76 77 78 79 80]
    #[ 10 209] [81 82 83 84 85 86 87 88 89]

    #conversion to numpy array
    arr = np.array(cv, copy = False)
    
    assert arr.size == 10
    for i in range(10):
        assert arr[i]['x'] == i+1

@pytest.mark.withdata
def test_read_clusters_and_fill_histogram(test_data_path): 
    # Create the histogram
    n_bins = 100
    xmin = -100
    xmax = 1e4
    hist_aare = bh.Histogram(bh.axis.Regular(n_bins, xmin, xmax))

    fname = test_data_path / "clust/beam_En700eV_-40deg_300V_10us_d0_f0_100.clust"

    #Read clusters and fill the histogram with pixel values
    with ClusterFile(fname, chunk_size = 10000) as f:
        for clusters in f:
            arr = np.array(clusters, copy = False)
            hist_aare.fill(arr['data'].flat)


    #Load the histogram from the pickle file
    with open(fname.with_suffix('.pkl'), 'rb') as f:
        hist_py = pickle.load(f)
        
    #Compare the two histograms
    assert hist_aare == hist_py