ClusterVector
================

The ClusterVector, holds clusters from the ClusterFinder. Since it is templated
in C++  we use a suffix indicating the type of cluster it holds. The suffix follows
the same pattern as for ClusterFile i.e. ``ClusterVector_Cluster3x3i``
for a vector holding 3x3 integer clusters.


At the moment the functionality from python is limited and it is not supported
to push_back clusters to the vector. The intended use case is to pass it to 
C++ functions that support the ClusterVector or to view it as a numpy array.

**View ClusterVector as numpy array**

.. code:: python

    from aare import ClusterFile
    with ClusterFile("path/to/file") as f:
        cluster_vector = f.read_frame()

    # Create a copy of the cluster data in a numpy array
    clusters = np.array(cluster_vector)

    # Avoid copying the data by passing copy=False
    clusters = np.array(cluster_vector, copy = False)


.. py:currentmodule:: aare

.. autoclass::  aare._aare.ClusterVector_Cluster3x3i
    :special-members: __init__
    :members:
    :undoc-members:
    :show-inheritance:
    :inherited-members: