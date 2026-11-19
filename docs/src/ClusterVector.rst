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
images. ``dequantize`` converts integer clusters to floating point and spreads
every value over the interval that was stored as that integer. Apply it before
calculating eta and before interpolating.

A gain can be applied in the same pass. When reading a file in chunks, create
the inverted gain map, the generator and the output vector once:

.. code:: cpp

    InvertedGainMap inverse_gain(gain_map);
    std::mt19937_64 generator(seed);
    ClusterVector<Cluster<float, 3, 3>> converted;

    ClusterFile<Cluster<int32_t, 3, 3>> file(path);
    for (auto &chunk : file.chunks(1'000'000)) {
        dequantize(chunk, converted, Quantization::Rounded, generator,
                   inverse_gain);
        auto eta = calculate_eta2(converted);
    }

.. doxygenenum:: aare::Quantization

.. doxygenfunction:: aare::dequantize(const ClusterVector<Cluster<T, ClusterSizeX, ClusterSizeY, CoordType>>&, ClusterVector<Cluster<OutT, ClusterSizeX, ClusterSizeY, CoordType>>&, const Quantization, std::mt19937_64&)

.. doxygenfunction:: aare::dequantize(const ClusterVector<Cluster<T, ClusterSizeX, ClusterSizeY, CoordType>>&, ClusterVector<Cluster<OutT, ClusterSizeX, ClusterSizeY, CoordType>>&, const Quantization, std::mt19937_64&, const InvertedGainMap&)

.. doxygenfunction:: aare::dequantize(const ClusterVector<Cluster<T, ClusterSizeX, ClusterSizeY, CoordType>>&, ClusterVector<Cluster<OutT, ClusterSizeX, ClusterSizeY, CoordType>>&, const Quantization, std::mt19937_64&, const double)

.. doxygenfunction:: aare::dequantize(const ClusterVector<Cluster<T, ClusterSizeX, ClusterSizeY, CoordType>>&, const Quantization, std::mt19937_64&)

.. doxygenfunction:: aare::dequantize(const ClusterVector<Cluster<T, ClusterSizeX, ClusterSizeY, CoordType>>&, const Quantization, std::optional<uint64_t>)
