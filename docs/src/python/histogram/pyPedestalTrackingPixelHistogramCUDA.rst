PedestalTrackingPixelHistogramCUDA
==================================

.. py:currentmodule:: aare

The GPU port of :py:class:`PedestalTrackingPixelHistogram`: one histogram of
``raw - pedestal`` per detector pixel, with the pedestal tracked during the
fill. Requires a build configured with ``-DAARE_CUDA=ON``. On a CPU-only
build the factory raises ``RuntimeError``.

With the default ``n_samples=1000`` the histograms, ``pedestal_mean()`` and
``pedestal_std()`` equal the CPU class's on the same frames. Frames are
``uint16``; passing another dtype raises ``TypeError``.

.. autofunction:: PedestalTrackingPixelHistogramCUDA

Returned class
--------------

One class per count dtype, named
``PedestalTrackingPixelHistogramCUDA_<counts>`` (``u16``, ``u32``). Both share
the API below. The methods from ``fill_histogram`` to ``window`` are those of
:py:class:`PixelHistogramCUDA_u16_u32`.

.. py:class:: PedestalTrackingPixelHistogramCUDA_u16

   Returned by :py:func:`PedestalTrackingPixelHistogramCUDA`.

   .. py:method:: push_pedestal_no_update(frames)

      Add raw frames to the pedestal seed: one frame ``(rows, cols)`` or
      several ``(n, rows, cols)``. The seed needs exactly ``n_samples``
      frames; the last one uploads the pedestal to the GPU. Raises
      ``RuntimeError`` for more.

   .. py:method:: update_mean()

      Set each pixel's sigma from its pedestal as tracked so far. The fill
      gates the pedestal update with it until the next call. Call it once
      after the seed. Raises ``RuntimeError`` before the seed is complete.

   .. py:method:: pedestal_mean()

      Pedestal mean of every pixel, ``(rows, cols)`` ``float32``, after every
      queued frame. Raises ``RuntimeError`` before the seed is complete.

   .. py:method:: pedestal_std()

      Sigma of every pixel as set by the last ``update_mean()``,
      ``(rows, cols)`` ``float32``; 0 before.

   .. py:property:: n_sigma

      Gate of the pedestal update: a value updates its pixel's pedestal if
      ``abs(residual) < n_sigma * sigma``. 0 or less: no tracking. Settable;
      a change applies to the frames queued after it.

   .. py:property:: n_samples

      Seed frames needed, and the weight ``1 / n_samples`` of a value in the
      update.

   .. py:property:: pedestal_frames

      Seed frames pushed so far.

   .. py:method:: fill_histogram(frames)

      Queue frames of shape ``(n_frames, rows, cols)``, in batches of
      ``batch_size``. Each value's residual is histogrammed and, within the
      gate, updates the pedestal, frame by frame.

   .. py:method:: values(out=None)

      Histogram of every queued frame, shape ``(rows, cols, n_bins)``.

   .. py:method:: values_pixel(row, col)

      Histogram of one pixel, shape ``(n_bins,)``.

   .. py:method:: flush()

      Wait until every queued frame has been histogrammed.

   .. py:method:: bin_edges()

      ``n_bins + 1`` edges from ``xmin`` to ``xmax``.

   .. py:method:: bin_centers()

      ``n_bins`` bin centres.

   .. py:method:: register_input_buffer(frames)

      Pin ``frames`` for faster copies.

   .. py:method:: unregister_input_buffer()

      Release the array pinned by ``register_input_buffer()``.

   .. py:method:: register_output_buffer(out)

      Pin ``out`` for ``values(out=out)``.

   .. py:method:: unregister_output_buffer()

      Release the array pinned by ``register_output_buffer()``.

   .. py:property:: rows

      Frame rows.

   .. py:property:: cols

      Frame columns.

   .. py:property:: n_bins

      Bins per pixel.

   .. py:property:: batch_size

      Frames copied and histogrammed per batch.

   .. py:property:: window

      True for the shared-memory window kernel, False for the direct kernel.
