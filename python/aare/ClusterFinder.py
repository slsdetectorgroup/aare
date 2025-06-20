from . import _aare
import numpy as np

_supported_cluster_sizes = [(2,2), (3,3), (5,5), (7,7), (9,9),]

def _type_to_char(dtype):
    if dtype == np.int32:
        return 'i'
    elif dtype == np.float32:
        return 'f'
    elif dtype == np.float64:
        return 'd'
    else:
        raise ValueError(f"Unsupported dtype: {dtype}. Only np.int32, np.float32, and np.float64 are supported.")

def _get_class(name, cluster_size, dtype):
    """
    Helper function to get the class based on the name, cluster size, and dtype.
    """
    try:
        class_name = f"{name}_Cluster{cluster_size[0]}x{cluster_size[1]}{_type_to_char(dtype)}"
        cls = getattr(_aare, class_name)
    except AttributeError:
        raise ValueError(f"Unsupported combination of type and cluster size: {dtype}/{cluster_size} when requesting {class_name}")
    return cls



def ClusterFinder(image_size, cluster_size, n_sigma=5, dtype = np.int32, capacity = 1024):
    """
    Factory function to create a ClusterFinder object. Provides a cleaner syntax for 
    the templated ClusterFinder in C++.
    """
    cls = _get_class("ClusterFinder", cluster_size, dtype)
    return cls(image_size, n_sigma=n_sigma, capacity=capacity)



def ClusterFinderMT(image_size, cluster_size = (3,3), dtype=np.int32, n_sigma=5, capacity = 1024, n_threads = 3): 
    """ 
    Factory function to create a ClusterFinderMT object. Provides a cleaner syntax for 
    the templated ClusterFinderMT in C++.
    """

    cls = _get_class("ClusterFinderMT", cluster_size, dtype)
    return cls(image_size, n_sigma=n_sigma, capacity=capacity, n_threads=n_threads)



def ClusterCollector(clusterfindermt, cluster_size = (3,3), dtype=np.int32): 
    """ 
    Factory function to create a ClusterCollector object. Provides a cleaner syntax for 
    the templated ClusterCollector in C++.
    """

    cls = _get_class("ClusterCollector", cluster_size, dtype)
    return cls(clusterfindermt)

def ClusterFileSink(clusterfindermt, cluster_file, dtype=np.int32): 
    """ 
    Factory function to create a ClusterCollector object. Provides a cleaner syntax for 
    the templated ClusterCollector in C++.
    """

    cls = _get_class("ClusterFileSink", clusterfindermt.cluster_size, dtype)
    return cls(clusterfindermt, cluster_file)


def ClusterFile(fname, cluster_size=(3,3), dtype=np.int32, chunk_size = 1000):
    """
    Factory function to create a ClusterFile object. Provides a cleaner syntax for
    the templated ClusterFile in C++.

    .. code-block:: python

        from aare import ClusterFile
        
        with ClusterFile("clusters.clust", cluster_size=(3,3), dtype=np.int32) as cf:
            # cf is now a ClusterFile_Cluster3x3i object but you don't need to know that.
            for clusters in cf:
                # Loop over clusters in chunks of 1000 
                # The type of clusters will be a ClusterVector_Cluster3x3i in this case

    """

    cls = _get_class("ClusterFile", cluster_size, dtype)
    return cls(fname, chunk_size=chunk_size)
