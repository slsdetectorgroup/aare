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

.. py:function:: dequantize(clusters, *, gain=None, truncated=False, seed=None, out=None)
   :noindex:

   Convert integer clusters to ``float32`` and replace every pixel value with a
   uniformly distributed value from the interval that was stored as that
   integer. Optionally divide by a gain in the same pass. Coordinates and the
   frame number are copied and the input is not changed.

   Integer pixel values make ratios such as eta take only rational values,
   which shows as spikes in finely binned eta histograms and as lines in
   interpolated images. Call ``dequantize`` before ``calculate_eta2`` and
   before ``Interpolator.interpolate``, and use the same converted clusters
   for both.

   .. code:: python

       from aare import ClusterFile, calculate_eta2, dequantize

       with ClusterFile("path/to/file") as f:
           for i, chunk in enumerate(f.chunks(1_000_000)):
               clusters = dequantize(chunk, gain=gain_map, seed=i)
               eta = calculate_eta2(clusters)

   Pass the gain here and do not call ``ClusterFile.set_gain_map`` as well:
   the gain map of the file is applied when reading and converts the result
   back to integers.

   ``truncated`` states how the writer of the clusters converted pixel values
   to integers. ``ClusterFinder`` rounds to the nearest integer, which is the
   default. Older cluster files were truncated toward zero and need
   ``truncated=True``; in such a file the pixel value 0 occurs about twice as
   often as 1 or -1:

   .. code:: python

       data = np.array(chunk)["data"]
       np.bincount(data[(data >= -3) & (data <= 3)] + 3)

   :param ClusterVector clusters: Clusters with an integer pixel type.
   :param gain: Gain in ADU/energy that the spread values are divided by:
      one number for all pixels, or a two-dimensional array indexed as
      ``[y, x]`` as for ``ClusterFile.set_gain_map``. Clusters that extend
      beyond the array are kept with all pixel values set to zero.
   :type gain: float or numpy.ndarray
   :param bool truncated: ``False`` spreads ``n`` over ``[n - 0.5, n + 0.5)``.
      ``True`` spreads positive ``n`` over ``[n, n + 1)``, negative ``n`` over
      ``(n - 1, n]`` and 0 over ``(-1, 1)``.
   :param int seed: Seed of the random generator; ``None`` seeds it from the
      system. Calls with the same seed add the same sequence, so use a
      different seed for every chunk.
   :param ClusterVector out: ``float32`` ClusterVector with the same cluster
      size that receives the result. Its allocation is reused when it is large
      enough; NumPy views of it become invalid.
   :return: The converted clusters; ``out`` when it was given.
   :rtype: ClusterVector
