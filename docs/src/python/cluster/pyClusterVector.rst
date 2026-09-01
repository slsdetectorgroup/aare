.. _py_clustervector:
    
ClusterVector
================

The ClusterVector, holds clusters from the ClusterFinder. Since it is templated
in C++  we use a suffix indicating the type of cluster it holds. The suffix follows
the same pattern as for ClusterFile i.e. ``ClusterVector_Cluster3x3i``
for a vector holding 3x3 integer clusters.


The intended use case is to pass a ClusterVector to C++ functions that support
it or to view it as a NumPy array.

**View ClusterVector as numpy array**

.. code:: python

    from aare import ClusterFile
    with ClusterFile("path/to/file") as f:
        cluster_vector = f.read_frame()

    # Create a copy of the cluster data in a numpy array
    clusters = np.array(cluster_vector)

    # Avoid copying the data by passing copy=False
    clusters = np.array(cluster_vector, copy = False)

.. warning::

   A NumPy array created with ``copy=False`` is a view of the ClusterVector's
   current storage. Do not call ``push_back`` or otherwise change the
   ClusterVector while using the view. A ``push_back`` that reallocates the
   backing buffer leaves existing NumPy views pointing to invalid memory, and
   appending without reallocation does not update their shape. Use
   ``copy=True`` if the ClusterVector may change after creating the array.


.. py:currentmodule:: aare

.. autoclass:: ClusterVector
    :members:
    :undoc-members:
    :inherited-members:

Below is the API of the ClusterVector_Cluster3x3i but all variants share the same API.

.. autoclass::  aare._aare.ClusterVector_Cluster3x3i
    :special-members: __init__, __call__ 
    :members:
    :undoc-members:
    :show-inheritance:
    :inherited-members:


**Free Functions:** 

.. autofunction:: reduce_to_3x3
   :noindex:

   Reduce a single Cluster to 3x3 by taking the 3x3 subcluster with highest photon energy.

.. autofunction:: reduce_to_2x2
   :noindex:

   Reduce a single Cluster to 2x2 by taking the 2x2 subcluster with highest photon energy.
