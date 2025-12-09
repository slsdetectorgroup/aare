
ClusterFile
============


The :class:`ClusterFile` class is the main interface to read and write clusters in aare. Unfortunately the
old file format does not include metadata like the cluster size and the data type. This means that the
user has to know this information from other sources. Specifying the wrong cluster size or data type
will lead to garbage data being read. 

.. py:currentmodule:: aare

.. autoclass:: ClusterFile
    :members:
    :undoc-members:
    :inherited-members:


Below is the API of the ClusterFile_Cluster3x3i but all variants share the same API.

.. autoclass:: aare._aare.ClusterFile_Cluster3x3i
    :special-members: __init__
    :members:
    :undoc-members:
    :show-inheritance:
    :inherited-members: