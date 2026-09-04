ClusterFinderCUDA
=================

GPU cluster finder. Available only when aare is configured with
``-DAARE_CUDA=ON``; on a CPU-only build the class is not compiled and the
Python factory raises ``RuntimeError``.

Single frames go through :cpp:func:`aare::ClusterFinderCUDA::find_clusters`,
which mirrors the CPU :cpp:class:`aare::ClusterFinder` interface. Throughput
comes from ``find_clusters_batched``, which distributes a batch of frames
round-robin over ``n_streams`` CUDA streams (4 by default) and overlaps H2D,
kernel and D2H work. Results are read back either with ``collect``, which
returns one ``ClusterVector`` per frame, or with ``collect_view``, which hands
back a ``BatchView`` reading clusters in place from the pinned host buffer with
no per-frame allocation or copy.

.. doxygenclass:: aare::ClusterFinderCUDA
   :members:
   :undoc-members:

Device kernel
-------------

.. doxygennamespace:: aare::device
   :members:
   :undoc-members:
