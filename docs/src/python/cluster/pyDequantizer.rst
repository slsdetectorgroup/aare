.. _py_dequantizer:

Dequantizer
===========

Converts integer clusters to ``float32`` clusters for eta and interpolation,
optionally applying a gain in the same pass.

.. code:: python

    from aare import ClusterFile, Dequantizer, calculate_eta2

    dequantizer = Dequantizer(gain=gain_map, seed=1)
    with ClusterFile("path/to/file") as f:
        for chunk in f.chunks(1_000_000):
            clusters = dequantizer(chunk)
            eta = calculate_eta2(clusters)

.. py:currentmodule:: aare

.. autoclass:: Dequantizer
   :members:
   :special-members: __call__
