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

   :param ClusterVector clustervector: Input clusters with both dimensions at
      least 3 and at least one dimension greater than 3.
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

Dequantizer
-----------

Integer pixel values make ratios such as eta take only rational values, which
shows as spikes in finely binned eta histograms and as lines in interpolated
images. A ``Dequantizer`` converts integer clusters to ``float32`` and replaces
every pixel value with a uniformly distributed value from the interval that
was stored as that integer. It can divide by a gain in the same pass. Convert
the clusters before ``calculate_eta2`` and before ``Interpolator.interpolate``,
and use the same converted clusters for both.

.. code:: python

    from aare import ClusterFile, Dequantizer, calculate_eta2

    dequantizer = Dequantizer(gain=gain_map, seed=1)
    with ClusterFile("path/to/file") as f:
        for chunk in f.chunks(1_000_000):
            clusters = dequantizer(chunk)
            eta = calculate_eta2(clusters)

Create one ``Dequantizer`` and call it for every chunk. It keeps the state of
its random generator between calls, so the result does not depend on the chunk
size. Do not call one object from several threads at the same time.

Pass the gain to the ``Dequantizer`` and do not call
``ClusterFile.set_gain_map``. That method is deprecated and emits a
``DeprecationWarning``: it converts the result back to the cluster type of
the file, which truncates integer clusters.

``truncated`` states how the writer of the clusters converted pixel values to
integers. ``ClusterFinder`` rounds to the nearest integer, which is the
default. Older cluster files were truncated toward zero and need
``truncated=True``; in such a file the pixel value 0 occurs about twice as
often as 1 or -1:

.. code:: python

    data = np.array(chunk)["data"]
    np.bincount(data[(data >= -3) & (data <= 3)] + 3)

.. py:class:: Dequantizer(*, gain=None, truncated=False, seed=None)
   :noindex:

   :param gain: Gain in ADU/energy that the spread values are divided by:
      one number for all pixels, or a two-dimensional array indexed as
      ``[y, x]``. The array is copied. Clusters that extend beyond the array
      are kept with all pixel values set to zero.
   :type gain: float or numpy.ndarray
   :param bool truncated: ``False`` spreads ``n`` over ``[n - 0.5, n + 0.5)``.
      ``True`` spreads positive ``n`` over ``[n, n + 1)``, negative ``n`` over
      ``(n - 1, n]`` and 0 over ``(-1, 1)``.
   :param int seed: Seed of the random generator; ``None`` seeds it from the
      system.

   .. py:method:: __call__(clusters)
      :noindex:

      Convert clusters. Coordinates and the frame number are copied and the
      input is not changed.

      :param ClusterVector clusters: Clusters with an integer pixel type.
      :return: ``float32`` clusters of the same cluster size.
      :rtype: ClusterVector
