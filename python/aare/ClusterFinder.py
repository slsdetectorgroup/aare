# SPDX-License-Identifier: MPL-2.0
from . import _aare
import numpy as np
from .factory import _type_to_char

_supported_cluster_sizes = [(2,2), (3,3), (5,5), (7,7), (9,9),]

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



def ClusterFinder(image_size, *, cluster_size=(3,3), n_sigma=5, dtype = np.int32, capacity = 1024, min_pedestal_samples = 1000):
    """
    Factory function to create a ClusterFinder object. Provides a cleaner syntax for 
    the templated ClusterFinder in C++.

    Parameters
    ----------
    image_size : tuple
        The size of the image as a tuple (height, width).
    cluster_size : tuple, optional
        The size of the cluster to find as a tuple (height, width). Default is (3,3).
    n_sigma : int, optional
        Multiplier of the standard deviation used as a threshold to identify potential photon pixels. Default is 5.
    dtype : data-type, optional
        The data type of the image. Default is np.int32.
    capacity : int, optional
        The maximum number of clusters than can be stored before reallocating. Default is 1024.
    min_pedestal_samples : int, optional
        The minimum number of pedestal samples to accumulate before using the pedestal. Default is 1000.
    """
    cls = _get_class("ClusterFinder", cluster_size, dtype)
    return cls(image_size, n_sigma=n_sigma, capacity=capacity, min_pedestal_samples=min_pedestal_samples)


def ClusterFinderMT(image_size, *, cluster_size = (3,3), dtype=np.int32, n_sigma=5, capacity = 1024, n_threads = 3, min_pedestal_samples = 1000): 
    """ 
    Factory function to create a ClusterFinderMT object. Provides a cleaner syntax for 
    the templated ClusterFinderMT in C++.

    Parameters
    ----------
    image_size : tuple
        The size of the image as a tuple (height, width).
    cluster_size : tuple, optional
        The size of the cluster to find as a tuple (height, width). Default is (3,3).
    n_sigma : int, optional
        Multiplier of the standard deviation used as a threshold to identify potential photon pixels. Default is 5.
    dtype : data-type, optional
        The data type of the image. Default is np.int32.
    capacity : int, optional
        The maximum number of clusters than can be stored before reallocating. Default is 1024.
    n_threads : int, optional
        The number of threads to use for processing. Default is 3.
    min_pedestal_samples : int, optional
        The minimum number of pedestal samples to accumulate before using the pedestal. Default is 1000.
    """

    cls = _get_class("ClusterFinderMT", cluster_size, dtype)
    return cls(image_size, n_sigma=n_sigma, capacity=capacity, min_pedestal_samples=min_pedestal_samples, n_threads=n_threads)


def ClusterCollector(clusterfindermt, dtype=np.int32): 
    """ 
    Factory function to create a ClusterCollector object. Provides a cleaner syntax for 
    the templated ClusterCollector in C++.
    """
    
    cls = _get_class("ClusterCollector", clusterfindermt.cluster_size, dtype)
    return cls(clusterfindermt)

def ClusterFileSink(clusterfindermt, cluster_file, dtype=np.int32): 
    """ 
    Factory function to create a ClusterCollector object. Provides a cleaner syntax for 
    the templated ClusterCollector in C++.
    """

    cls = _get_class("ClusterFileSink", clusterfindermt.cluster_size, dtype)
    return cls(clusterfindermt, cluster_file)


def ClusterFile(fname, cluster_size=(3,3), dtype=np.int32, chunk_size = 1000, mode = "r"):
    """Create a reader or writer for a legacy binary cluster file.

    Parameters
    ----------
    fname : path-like
        Cluster file to open.
    cluster_size : tuple[int, int], default=(3, 3)
        Cluster dimensions stored in the file.
    dtype : numpy dtype, default=numpy.int32
        Data type of the cluster values stored in the file.
    chunk_size : int, default=1000
        Maximum number of selected clusters returned by ``chunks()`` and
        default iteration. Must be positive when iterating over chunks.
    mode : {"r", "w", "a"}, default="r"
        Open for reading, truncate and write, or append, respectively.

    Returns
    -------
    ClusterFile
        The compiled ClusterFile specialization matching ``cluster_size`` and
        ``dtype``.

    Notes
    -----
    The file format contains no cluster shape or data-type metadata. Supplying
    values that do not match the file causes its bytes to be interpreted
    incorrectly. Use ``frames()`` to iterate over complete frames, including
    empty or fully filtered frames with their stored frame numbers. Use
    ``chunks()`` or ``chunks(chunk_size)`` to iterate over selected clusters
    in batches. Chunks may combine frames, so their frame number is not
    reliable per-cluster metadata.

    Iterators consume the current file position without rewinding; use one
    traversal at a time. Each result owns its storage and remains valid after
    advancing the iterator or closing the file.

    Examples
    --------

    .. code-block:: python

        from aare import ClusterFile

        with ClusterFile(
            "clusters.clust", cluster_size=(3, 3), dtype=np.int32
        ) as cf:
            for clusters in cf.chunks():
                # Process clusters in chunks of at most 1000.
                ...

    """

    cls = _get_class("ClusterFile", cluster_size, dtype)
    return cls(fname, chunk_size=chunk_size, mode=mode)
