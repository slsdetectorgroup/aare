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

``Quantization`` states how the writer of the clusters converted pixel values
to integers, which decides the interval each value is drawn from:

- ``Quantization::Rounded`` (default): rounded to the nearest integer, as
  ``ClusterFinder`` does. ``n`` is spread over ``[n - 0.5, n + 0.5)``.
- ``Quantization::Truncated``: truncated toward zero, as in older cluster
  files. Positive ``n`` is spread over ``[n, n + 1)``, negative ``n`` over
  ``(n - 1, n]`` and 0 over ``(-1, 1)``. In such a file the pixel value 0
  occurs about twice as often as 1 or -1.

.. doxygenenum:: aare::Quantization

.. doxygenclass:: aare::Dequantizer
   :members:
