.. _Dequantizer_C++API:

Dequantizer
===========

Converts integer clusters to floating point clusters for eta and
interpolation, optionally applying a gain in the same pass.

.. code:: cpp

    Dequantizer dequantizer(gain_map.view(), Quantization::Rounded, seed);

    ClusterFile<Cluster<int32_t, 3, 3>> file(path);
    for (auto &chunk : file.chunks(1'000'000)) {
        auto clusters = dequantizer(chunk); // Cluster<float, 3, 3>
        auto eta = calculate_eta2(clusters);
    }

.. doxygenenum:: aare::Quantization

.. doxygenclass:: aare::Dequantizer
   :members:
