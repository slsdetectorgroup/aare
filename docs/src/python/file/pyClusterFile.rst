
ClusterFile
===========

The :func:`ClusterFile` factory is the main interface for reading and writing
legacy cluster files. Use ``mode="r"`` to read, ``mode="w"`` to truncate and
write, or ``mode="a"`` to append.

The format does not store the cluster dimensions, value type, coordinate type,
padding, or byte order. The ``cluster_size`` and ``dtype`` arguments must match
the writer; otherwise the bytes are interpreted incorrectly.

Use ``frames()`` when frame boundaries and the frame number matter:

.. code-block:: python

    with ClusterFile("clusters.clust") as file:
        for frame in file.frames():
            print(frame.frame_number, frame.size)

Each stored frame is yielded, including empty frames and frames whose clusters
are all rejected by filtering. Missing frame numbers are not synthesized.
The explicit ``read_frame()`` method returns one frame or ``None`` at EOF.

Use ``chunks()`` to iterate with the constructor's chunk size, or pass an
explicit size for that traversal:

.. code-block:: python

    with ClusterFile("clusters.clust", chunk_size=1000) as file:
        for clusters in file.chunks(10000):
            process(clusters)

Chunk sizes must be positive and count selected clusters after filtering.
``file.chunks(10000)`` does not change the default size. Iterating directly
over ``file`` or calling ``next(file)`` still reads chunks using that default.
Chunks and ``read_clusters(n_clusters)`` may split or combine frames, so their
frame numbers are not reliable per-cluster metadata. Empty chunks are not
yielded.
Chunk reads and iteration raise an error if they encounter an incomplete frame
header or cluster record, including when ROI or noise filtering is enabled.
They return fewer clusters than requested only at a clean end of file.

Both iterators consume the current file position without rewinding. Creating
an iterator does not read; each ``next()`` reads one result without prefetching
the following result. Use one traversal at a time and separate readers for
independent cursors. Switching to frames after a chunk stops partway through a
frame raises an error until the remaining clusters in that frame have been
read. Exhausted iterators continue to raise ``StopIteration``.

Iterators keep their file object alive, but explicitly closing the file or
leaving its ``with`` block prevents further reads. Every yielded
``ClusterVector`` owns its storage and can be retained after iteration advances
or the file closes. NumPy views also remain valid across those operations;
as usual, resizing or modifying the allocation of their backing vector can
invalidate them.

When a gain map is configured, it is applied to every cluster whose complete
footprint lies inside the map. Clusters whose footprint extends beyond the
gain-map boundaries remain in the returned cluster vector, but all their data
values are set to zero.

.. py:currentmodule:: aare

.. autofunction:: ClusterFile


Below is the API of ``ClusterFile_Cluster3x3i``; all compiled variants share the
same API.

.. autoclass:: aare._aare.ClusterFile_Cluster3x3i
    :special-members: __init__
    :members:
    :undoc-members:
    :show-inheritance:
    :inherited-members:
