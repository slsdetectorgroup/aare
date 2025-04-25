
from ._aare import ClusterFinder_Cluster3x3i, ClusterFinder_Cluster2x2i, ClusterFinderMT_Cluster3x3i, ClusterFinderMT_Cluster2x2i, ClusterCollector_Cluster3x3i, ClusterCollector_Cluster2x2i


from ._aare import ClusterFileSink_Cluster3x3i, ClusterFileSink_Cluster2x2i
import numpy as np

def ClusterFinder(image_size, cluster_size, n_sigma=5, dtype = np.int32, capacity = 1024):
    """
    Factory function to create a ClusterFinder object. Provides a cleaner syntax for 
    the templated ClusterFinder in C++.
    """
    if dtype == np.int32 and cluster_size == (3,3):
        return ClusterFinder_Cluster3x3i(image_size, n_sigma = n_sigma, capacity=capacity)
    elif dtype == np.int32 and cluster_size == (2,2): 
        return ClusterFinder_Cluster2x2i(image_size, n_sigma = n_sigma, capacity=capacity)
    else:
        #TODO! add the other formats
        raise ValueError(f"Unsupported dtype: {dtype}. Only np.int32 is supported.")


def ClusterFinderMT(image_size, cluster_size = (3,3), dtype=np.int32, n_sigma=5, capacity = 1024, n_threads = 3): 
    """ 
    Factory function to create a ClusterFinderMT object. Provides a cleaner syntax for 
    the templated ClusterFinderMT in C++.
    """

    if dtype == np.int32 and cluster_size == (3,3):
        return ClusterFinderMT_Cluster3x3i(image_size, n_sigma = n_sigma,
                    capacity = capacity, n_threads = n_threads)
    elif dtype == np.int32 and cluster_size == (2,2):
        return ClusterFinderMT_Cluster2x2i(image_size, n_sigma = n_sigma,
                    capacity = capacity, n_threads = n_threads)
    else:
        #TODO! add the other formats
        raise ValueError(f"Unsupported dtype: {dtype}. Only np.int32 is supported.")


def ClusterCollector(clusterfindermt, cluster_size = (3,3), dtype=np.int32): 
    """ 
    Factory function to create a ClusterCollector object. Provides a cleaner syntax for 
    the templated ClusterCollector in C++.
    """

    if dtype == np.int32 and cluster_size == (3,3): 
        return ClusterCollector_Cluster3x3i(clusterfindermt)
    elif dtype == np.int32 and cluster_size == (2,2):
        return ClusterCollector_Cluster2x2i(clusterfindermt)
     
    else: 
        #TODO! add the other formats
        raise ValueError(f"Unsupported dtype: {dtype}. Only np.int32 is supported.")

def ClusterFileSink(clusterfindermt, cluster_file, dtype=np.int32): 
    """ 
    Factory function to create a ClusterCollector object. Provides a cleaner syntax for 
    the templated ClusterCollector in C++.
    """

    if dtype == np.int32 and clusterfindermt.cluster_size == (3,3): 
        return ClusterFileSink_Cluster3x3i(clusterfindermt, cluster_file)
    elif dtype == np.int32 and clusterfindermt.cluster_size == (2,2):
        return ClusterFileSink_Cluster2x2i(clusterfindermt, cluster_file)
     
    else: 
        #TODO! add the other formats
        raise ValueError(f"Unsupported dtype: {dtype}. Only np.int32 is supported.")