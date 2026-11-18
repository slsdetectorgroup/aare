PedestalTrackingPixelHistogramCUDA
==================================

The GPU port of ``PedestalTrackingPixelHistogram``: one histogram of
``raw - pedestal`` per detector pixel, with the pedestal tracked during the
fill. Available only when aare is configured with ``-DAARE_CUDA=ON``.

1. ``push_pedestal_no_update()`` with exactly ``n_samples`` raw frames
   (default 1000, as the CPU class) accumulates the pedestal on the host and
   uploads it to the GPU with the last frame.
2. ``update_mean()`` sets each pixel's sigma from its pedestal. It stays fixed
   until the next ``update_mean()``.
3. ``fill_histogram()`` histograms ``float(raw) - mean`` for every value and,
   for a value with ``|residual| < n_sigma * sigma``, updates the pixel's
   pedestal with ``raw`` (``FastPedestal``'s moving average, weight
   ``1 / n_samples``), frame by frame. ``n_sigma <= 0`` disables the
   tracking. A mask skips pixels entirely.

The pedestal is kept in ``double`` and every update is rounded as on the
CPU, so with ``n_samples = 1000`` the histograms, ``pedestal_mean()`` and
``pedestal_std()`` equal the CPU class's bit for bit.

The class runs on the pipeline of :doc:`PixelHistogramCUDA` (batched copies
overlapping the fill kernel, the shared-memory window kernel), with a fill
policy that carries each pixel's pedestal in registers through a batch.
The fill, readout and pinning methods are documented there.

.. doxygenclass:: aare::cuda::PedestalTrackingPixelHistogramCUDA
   :members:
   :undoc-members:
