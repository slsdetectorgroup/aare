# SPDX-License-Identifier: MPL-2.0
from . import _aare
from ._aare import read_state_header
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


def _load_state(filename, dtype=None, capacity=1024):
    """Create a ClusterFinder from a state file.

    The file is written by ``save_state()`` of a cluster finder and holds its
    pedestal, ``nSigma`` and threshold, so the new finder continues exactly
    where the stored one stopped, whether its pedestal was still initializing
    or ready. The image shape, the cluster size, ``nSigma``,
    ``min_pedestal_samples`` and by default the dtype are taken from the
    file. Clusters that the stored finder had found but not handed out with
    ``steal_clusters()`` are not in the file.

    This returns a new finder. To load a state file into a finder that
    already exists, call ``load_state(filename)`` on that finder; it keeps
    its own dtype and must have the cluster size stored in the file.

    Parameters
    ----------
    filename : path-like
        State file written by ``save_state()``.
    dtype : data-type, optional
        The data type of the clusters. By default that of the finder that
        wrote the file, as stored in it.
    capacity : int, optional
        The maximum number of clusters that can be stored before
        reallocating. Default is 1024.

    Raises
    ------
    RuntimeError
        If the file cannot be read, is not a state file of a ClusterFinder,
        or is truncated or damaged.
    ValueError
        If there is no ClusterFinder with the stored cluster size and the
        dtype, given or stored.
    """
    cluster_size, stored_dtype = _stored_cluster_type(filename)
    if dtype is None:
        dtype = stored_dtype
    cls = _get_class("ClusterFinder", cluster_size, dtype)
    return cls.from_state_file(filename, capacity=capacity)


def _stored_cluster_type(filename):
    """Return the cluster size and dtype of the finder that wrote a state file.

    Only files that are not state files are reported here. Anything else
    that is wrong with the file is left to ``from_state_file()``, which
    checks everything it reads; a 3x3 ``int32`` finder only serves to reach
    it.
    """
    header = read_state_header(filename)
    try:
        if header["type"] == "ClusterFinder":
            state = header["state"]
            cluster_size = (state["cluster_size_x"], state["cluster_size_y"])
            return cluster_size, np.dtype(state["cluster_dtype"])
    except (KeyError, TypeError):
        pass
    return (3, 3), np.int32


ClusterFinder.load_state = _load_state


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
    """Create a reader or writer for a binary cluster file.

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
