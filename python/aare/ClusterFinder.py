# SPDX-License-Identifier: MPL-2.0
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
    elif dtype == np.int16:
        return 'i16'
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



def ClusterFinder(image_size, cluster_size=(3,3), n_sigma=5, dtype = np.int32, capacity = 1024):
    """
    Factory function to create a ClusterFinder object. Provides a cleaner syntax for 
    the templated ClusterFinder in C++.
    """
    cls = _get_class("ClusterFinder", cluster_size, dtype)
    return cls(image_size, n_sigma=n_sigma, capacity=capacity)



def ClusterFinderFrozen(image_size, cluster_size=(3,3), n_sigma=5, dtype=np.int32, capacity=1024):
    """
    Factory function to create a ClusterFinderFrozen object.

    Diagnostic twin of ClusterFinder: identical decision logic, but the pedestal
    is frozen per frame (every decision reads a start-of-frame snapshot, and all
    pedestal updates are deferred to the end of the frame). This mirrors the CUDA
    kernel's per-frame update model, so running it against ClusterFinderCUDA
    isolates pedestal-update timing as the sole variable.
    """
    cls = _get_class("ClusterFinderFrozen", cluster_size, dtype)
    return cls(image_size, n_sigma=n_sigma, capacity=capacity)


def ClusterFinderMT(image_size, cluster_size = (3,3), dtype=np.int32, n_sigma=5, capacity = 1024, n_threads = 3):
    """ 
    Factory function to create a ClusterFinderMT object. Provides a cleaner syntax for 
    the templated ClusterFinderMT in C++.
    """

    cls = _get_class("ClusterFinderMT", cluster_size, dtype)
    return cls(image_size, n_sigma=n_sigma, capacity=capacity, n_threads=n_threads)


def _cuda_available():
    """True if this build of aare was compiled with -DAARE_CUDA=ON."""
    return hasattr(_aare, "ClusterFinderCUDA_Cluster3x3i")


def ClusterFinderCUDA(image_size, cluster_size=(3,3), n_sigma=5, dtype=np.int32,
                      max_clusters_per_frame=2048, n_streams=4,
                      time_kernels=False):
    """
    Factory function to create a ClusterFinderCUDA object. Provides a cleaner
    syntax for the templated ClusterFinderCUDA in C++. API mirrors
    ClusterFinder() plus CUDA-specific knobs.

    Parameters
    ----------
    image_size : tuple of (int, int)
        Detector shape as (nrows, ncols).
    cluster_size : tuple of (int, int), optional
        Cluster window size; default (3, 3).
    n_sigma : float, optional
        Threshold in units of per-pixel pedestal standard deviation.
    dtype : numpy dtype, optional
        Cluster value type (np.int32 or np.float32).
    max_clusters_per_frame : int, optional
        Tight upper bound on clusters per frame. Determines the fixed-size D2H
        transfer per frame. Set this high enough to never truncate real frames
        but as tight as possible to minimize PCIe traffic. Default 2048.
    n_streams : int, optional
        Number of CUDA streams for H2D/kernel/D2H pipelining. Default 4.
    time_kernels : bool, optional
        Enable per-frame CUDA-event kernel timing, exposed via
        avg_kernel_time_ms(). Off by default because it adds two event records
        per frame to the streams plus a host-side query per frame, and the
        number it yields is only meaningful at n_streams=1 — under multi-stream
        contention the events measure queue wait, not execution, and over-read
        by up to ~3.5x. Use Nsight Systems for exclusive kernel times. When
        disabled, avg_kernel_time_ms() returns NaN.

    Example
    -------
    .. code-block:: python

        from aare import ClusterFinderCUDA

        cf = ClusterFinderCUDA(image_size=(400, 400),
                               cluster_size=(3, 3),
                               n_sigma=5,
                               n_streams=4)
        for frame in pedestal_frames:
            cf.push_pedestal_frame(frame)

        # Batched (recommended for throughput)
        results = cf.find_clusters_batched(frames_3d, first_frame=0)

        # Or single-frame (one launch per frame)
        for i, frame in enumerate(data_frames):
            cf.find_clusters(frame, frame_number=i)
            clusters = cf.steal_clusters()
    """
    if not _cuda_available():
        raise RuntimeError(
            "ClusterFinderCUDA is not available in this build of aare. "
            "Rebuild with -DAARE_CUDA=ON (and -DAARE_PYTHON_BINDINGS=ON)."
        )

    cls = _get_class("ClusterFinderCUDA", cluster_size, dtype)
    return cls(image_size,
               n_sigma=n_sigma,
               max_clusters_per_frame=max_clusters_per_frame,
               n_streams=n_streams,
               time_kernels=time_kernels)

def find_cluster_views_batched_iter(cf, frames, first_frame=0, chunk=None):
    """
    Drive a ClusterFinderCUDA over `frames`, yielding zero-copy BatchViews.

    Same pipelining as cf.find_clusters_batched() — chunk i+1 is submitted
    before chunk i is collected — but nothing is copied out of the pinned D2H
    buffer, so the host cost per frame collapses to reading one counter. At 9x9
    that removes ~467 kB of copying per frame.

    Each view is released as soon as the loop body finishes, which is what makes
    the next submit legal (the finder has only two slots). Consequently:

    **Anything you need after the loop body must be copied out.** Reductions
    (`v.sums()`) return owned numpy arrays and are safe; `v.frame_data(i)` and
    `v.frame_xy(i)` are views and are not.

    Parameters
    ----------
    cf : ClusterFinderCUDA
    frames : ndarray (n_frames, nrows, ncols), uint16
        Pin it first with cf.register_input_buffer(frames) for DMA-speed H2D.
    first_frame : int
        Frame number of frames[0].
    chunk : int, optional
        Frames per chunk; defaults to cf.chunk_size_for(len(frames)).

    Yields
    ------
    BatchView
        Valid only until the next iteration.

    Example
    -------
    .. code-block:: python

        cf.register_input_buffer(data)
        for v in find_cluster_views_batched_iter(cf, data):
            hist.fill(v.sums())          # reduced in C++, nothing materialised
        cf.unregister_input_buffer()
    """
    n = frames.shape[0]
    if n == 0:
        return
    c = chunk or cf.chunk_size_for(n)
    bounds = [(s, min(s + c, n)) for s in range(0, n, c)]

    tok = cf.submit_batch(frames[bounds[0][0]:bounds[0][1]],
                          first_frame=first_frame + bounds[0][0])
    for a, b in bounds[1:]:
        nxt = cf.submit_batch(frames[a:b], first_frame=first_frame + a)
        view = cf.collect_view(tok)
        try:
            yield view
        finally:
            view.release()          # frees the slot for the submit after next
        tok = nxt
    view = cf.collect_view(tok)
    try:
        yield view
    finally:
        view.release()


def ClusterFinderCUDAGraph(image_size, cluster_size=(3,3), n_sigma=5, dtype=np.int32,
                           max_clusters_per_frame=2048, n_streams=4):
    """
    Factory function to create a ClusterFinderCUDAGraph object. Uses pre-recorded
    CUDA Graphs to reduce per-frame CPU API overhead (~23 µs vs ~31 µs for the
    stream-based version), potentially improving throughput when processing is
    CPU-overhead-bound.

    Parameters
    ----------
    image_size : tuple of (int, int)
        Detector shape as (nrows, ncols).
    cluster_size : tuple of (int, int), optional
        Cluster window size; default (3, 3).
    n_sigma : float, optional
        Threshold in units of per-pixel pedestal standard deviation.
    dtype : numpy dtype, optional
        Cluster value type (np.int32 or np.float32).
    max_clusters_per_frame : int, optional
        Hard upper bound on clusters per frame. Default 2048.
    n_streams : int, optional
        Number of CUDA streams (one graph per stream). Default 4.

    Note
    ----
    avg_kernel_time_ms() always returns 0.0 for this variant — use
    wall-clock timing around find_clusters_batched() instead.
    """
    if not _cuda_available():
        raise RuntimeError(
            "ClusterFinderCUDAGraph is not available in this build of aare. "
            "Rebuild with -DAARE_CUDA=ON (and -DAARE_PYTHON_BINDINGS=ON)."
        )

    cls = _get_class("ClusterFinderCUDAGraph", cluster_size, dtype)
    return cls(image_size,
               n_sigma=n_sigma,
               max_clusters_per_frame=max_clusters_per_frame,
               n_streams=n_streams)


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
    return cls(fname, chunk_size=chunk_size, mode=mode)
