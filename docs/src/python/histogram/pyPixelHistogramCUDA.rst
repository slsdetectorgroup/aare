PixelHistogramCUDA
==================

.. py:currentmodule:: aare

One histogram per detector pixel, filled on the GPU. Requires a build
configured with ``-DAARE_CUDA=ON``. On a CPU-only build the factory raises
``RuntimeError``.

The factory picks a class by frame dtype (``uint16``, ``float32`` or
``float64``) and count dtype (``storage_dtype``, as for ``PixelHistogram``).
Frames are never converted: passing another dtype raises ``TypeError``.

.. autofunction:: PixelHistogramCUDA

Returned class
--------------

One class per frame and count dtype, named
``PixelHistogramCUDA_<frames>_<counts>`` (frames ``u16``, ``f``, ``d``;
counts ``u8``, ``u16``, ``u32``, ``u64``, ``f``, ``d``). All share the API
below, shown for ``uint16`` frames and the default ``uint32`` counts.

.. py:class:: PixelHistogramCUDA_u16_u32

   Returned by :py:func:`PixelHistogramCUDA`.

   .. py:method:: fill_histogram(frames)

      Queue frames of shape ``(n_frames, rows, cols)``, in batches of
      ``batch_size``. Returns once the work is queued. Frames in a registered
      buffer are copied in the background: don't modify them until
      ``flush()`` or ``values()`` has returned.

   .. py:method:: values(out=None)

      Histogram of every queued frame, shape ``(rows, cols, n_bins)``, counts
      capped at the storage dtype's maximum. Waits for the queued work;
      filling can continue afterwards. With ``out``, writes into that array
      and returns it; pin it with ``register_output_buffer()`` for the fastest
      copy.

   .. py:method:: values_pixel(row, col)

      Histogram of one pixel, shape ``(n_bins,)``. For many pixels, call
      ``values()`` once instead. Raises ``IndexError`` outside the frame.

   .. py:method:: flush()

      Wait until every queued frame has been histogrammed.

   .. py:method:: bin_edges()

      ``n_bins + 1`` edges from ``xmin`` to ``xmax``.

   .. py:method:: bin_centers()

      ``n_bins`` bin centres.

   .. py:method:: register_input_buffer(frames)

      Pin ``frames`` so ``fill_histogram()`` copies from it (and from its
      slices) at full bandwidth, overlapping copies and kernels. Keeps the
      array alive; registering another one releases it. An array can be
      registered by only one histogram at a time (a CUDA limit).

   .. py:method:: unregister_input_buffer()

      Release the array pinned by ``register_input_buffer()``.

   .. py:method:: register_output_buffer(out)

      Pin ``out`` (storage dtype, shape ``(rows, cols, n_bins)``) for
      ``values(out=out)``. Same rules as ``register_input_buffer()``.

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

      True for the shared-memory window kernel (each pixel's 128 bins around
      its noise peak counted on chip), False for the direct kernel.

   .. py:property:: dtype

      dtype of the frames this histogram takes.
