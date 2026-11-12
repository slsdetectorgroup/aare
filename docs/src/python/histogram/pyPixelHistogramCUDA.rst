PixelHistogramCUDA
==================

.. py:currentmodule:: aare

One histogram per detector pixel, filled on the GPU. Requires a build
configured with ``-DAARE_CUDA=ON``. On a CPU-only build the factory raises
``RuntimeError``.

The factory picks a class by frame dtype (``uint16``, ``float32`` or
``float64``) and count dtype (``storage_dtype``, as for ``PixelHistogram``).
Frames are never converted: passing another dtype raises ``TypeError``.

The returned object has:

* ``fill_histogram(frames)``: queue frames of shape ``(n_frames, rows, cols)``;
  returns before the work is done.
* ``values(out=None)``: the histogram of every queued frame, shape
  ``(rows, cols, n_bins)``; waits for the queued work. Filling can continue
  afterwards.
* ``values_pixel(row, col)``, ``bin_edges()``, ``bin_centers()``, ``flush()``.
* ``register_input_buffer(frames)`` and ``register_output_buffer(out)``: pin
  the arrays for faster copies (each array in one histogram at a time), and
  the matching ``unregister_*`` methods.
* Read-only properties ``rows``, ``cols``, ``n_bins``, ``batch_size``,
  ``window`` and ``dtype``.

.. autofunction:: PixelHistogramCUDA
