
from ._aare import ClusterFinder_Cluster3x3i
import numpy as np

def ClusterFinder(image_size, cluster_size, n_sigma=5, dtype = np.int32, capacity = 1024):
    """
    Factory function to create a ClusterFinder object. Provides a cleaner syntax for 
    the templated ClusterFinder in C++.
    """
    if dtype == np.int32 and cluster_size == (3,3):
        return ClusterFinder_Cluster3x3i(image_size, n_sigma = n_sigma, capacity=capacity)
    else:
        #TODO! add the other formats
        raise ValueError(f"Unsupported dtype: {dtype}. Only np.int32 is supported.")
