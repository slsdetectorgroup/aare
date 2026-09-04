
ClusterFile
===========

The :func:`ClusterFile` factory is the main interface for reading and writing
legacy cluster files. Use ``mode="r"`` to read, ``mode="w"`` to truncate and
write, or ``mode="a"`` to append.

The format does not store the cluster dimensions, value type, coordinate type,
padding, or byte order. The ``cluster_size`` and ``dtype`` arguments must match
the writer; otherwise the bytes are interpreted incorrectly.

Use ``read_frame()`` when frame boundaries and the frame number matter. It
returns ``None`` at the end of the file and raises an error for an incomplete
frame.
Iteration and ``read_clusters()`` return chunks of up to ``chunk_size``
selected clusters. A chunk may combine frames, so its frame number is not
reliable per-cluster metadata.

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
