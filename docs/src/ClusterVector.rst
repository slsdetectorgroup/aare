ClusterVector
=============

.. doxygenclass:: aare::ClusterVector
   :members:
   :undoc-members:
   :private-members:



.. doxygenclass:: aare::ClusterVector< Cluster< T, ClusterSizeX, ClusterSizeY, CoordType > > 
   :members:
   :undoc-members:
   :private-members:
   

**Free Functions:** 

.. doxygenfunction:: aare::reduce_to_3x3(const ClusterVector<Cluster<T, ClusterSizeX, ClusterSizeY, CoordType>>&)
   
.. doxygenfunction:: aare::reduce_to_2x2(const ClusterVector<Cluster<T, ClusterSizeX, ClusterSizeY, CoordType>>&)
   

**Dequantization:**

Integer pixel values make ratios such as eta take only rational values, which
shows as spikes in finely binned eta histograms and as lines in interpolated
images. A ``Dequantizer`` converts integer clusters to floating point and
spreads every value over the interval that was stored as that integer. Convert
the clusters before calculating eta and before interpolating.

A gain can be applied in the same pass. Create one ``Dequantizer`` and use it
for every chunk of a file: it keeps the state of its random generator between
calls, so the result does not depend on the chunk size.

.. code:: cpp

    Dequantizer dequantizer(gain_map.view(), Quantization::Rounded, seed);

    ClusterFile<Cluster<int32_t, 3, 3>> file(path);
    for (auto &chunk : file.chunks(1'000'000)) {
        auto clusters = dequantizer(chunk); // Cluster<float, 3, 3>
        auto eta = calculate_eta2(clusters);
    }

``ClusterFile::set_gain_map`` is deprecated: it converts the result back to
the cluster type of the file, which truncates integer clusters.

.. doxygenenum:: aare::Quantization

.. doxygenclass:: aare::Dequantizer
   :members:
