PixelHistogramCUDA
==================

One histogram per detector pixel, filled on the GPU. Available only when aare
is configured with ``-DAARE_CUDA=ON``; on a CPU-only build the class is not
compiled and the Python factory raises ``RuntimeError``.

Frames are copied to the GPU in batches of ``batch_size``, with the type the
caller gives them (``uint16`` frames move half the bytes of ``float32``), and
histogrammed into one accumulator that stays on the GPU for the whole run.
The copy of one batch overlaps the kernel on the previous one, so the fill
runs as fast as the slower of the two. With the default fill kernel the copy
is the slower one. :cpp:func:`aare::cuda::PixelHistogramCUDA::values` caps the
counts at the storage type's maximum and copies the histogram back, shape
``(rows, cols, n_bins)``; it can be called at any time, and filling can
continue afterwards.

Two fill kernels give identical counts:

* ``window = true`` (default): during a batch, each pixel's 128 consecutive
  bins around its noise peak are counted in shared memory; only values outside
  them (mostly photons) go straight to the histogram in GPU memory. The window
  places itself and follows the peak. Needs ``batch_size <= 65535``.
* ``window = false``: every value goes straight to GPU memory. Slower for fine
  bins.

The counts equal those of the CPU ``PixelHistogram`` with the same axis type:
``float`` for integer frames, the frame type otherwise. With another axis type
a value lying on a bin edge can land in the neighbouring bin.

GPU memory: the accumulator (``rows * cols * n_bins`` device counters), two
frame batches (``2 * batch_size * rows * cols`` frame values), and during
``values()`` a temporary histogram of the storage type unless it equals the
device counter type. Lower ``batch_size`` for large frames.

Pinning the input with ``register_input_buffer`` (and the output with
``register_output_buffer``) makes the copies run at full bandwidth. A buffer
can be registered by only one object at a time. The class is not thread-safe.

.. doxygenclass:: aare::cuda::PixelHistogramCUDA
   :members:
   :undoc-members:
