.. _py_clustervector:
    
ClusterVector
================

A ClusterVector stores fixed-size clusters contiguously. Since it is templated
in C++, each bound class has a suffix indicating the cluster type. The suffix
follows the same pattern as ClusterFile; for example,
``ClusterVector_Cluster3x3i`` stores 3x3 clusters with 32-bit integer pixels.


The intended use case is to pass a ClusterVector to C++ functions that support
it or to view it as a NumPy array.

**View ClusterVector as a NumPy array**

.. code:: python

    from aare import ClusterFile
    with ClusterFile("path/to/file") as f:
        cluster_vector = f.read_frame()

    # Create a copy of the cluster data in a numpy array
    clusters = np.array(cluster_vector)

    # Avoid copying the data by passing copy=False
    clusters = np.array(cluster_vector, copy=False)

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

Below is the API of ``ClusterVector_Cluster3x3i``. All variants share the same
API.

.. autoclass::  aare._aare.ClusterVector_Cluster3x3i
    :special-members: __init__, __call__ 
    :members:
    :undoc-members:
    :show-inheritance:
    :inherited-members:


**Free Functions:**

.. py:function:: hitmap(image_size, clusters)
   :noindex:

   Count cluster centers into an ``int32`` image. ``image_size`` is given as
   ``(rows, columns)``, and output element ``[y, x]`` contains the number of
   photon hits at that coordinate. Out-of-bounds hits are ignored. All registered
   ClusterVector variants are accepted.

   :param tuple[int, int] image_size: Shape of the output image.
   :param ClusterVector clusters: Clusters whose centers are counted.
   :return: Hit counts with shape ``image_size``.
   :rtype: numpy.ndarray

.. py:function:: reduce_to_3x3(clustervector)
   :noindex:

   Return a new vector containing the central 3x3 block of every input cluster.
   Cluster order, coordinates, frame number, and pixel dtype are preserved.

   :param ClusterVector clustervector: Input clusters with odd dimensions of 3x3 or larger.
   :return: Reduced 3x3 clusters.
   :rtype: ClusterVector

.. py:function:: reduce_to_2x2(clustervector)
   :noindex:

   Return a new vector containing the highest-sum center-adjacent 2x2 block of
   every input cluster. Cluster order, coordinates, frame number, and pixel
   dtype are preserved.

   :param ClusterVector clustervector: Input clusters of size 2x2 or larger.
   :return: Reduced 2x2 clusters.
   :rtype: ClusterVector
