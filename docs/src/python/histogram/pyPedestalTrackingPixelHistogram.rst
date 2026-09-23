PedestalTrackingPixelHistogram
==============================

.. warning::

    ``PedestalTrackingPixelHistogram`` is specifically designed for use in the Jungfrau calibration
    pipeline. Make sure you understand the behaviour before using it in other contexts.

``PedestalTrackingPixelHistogram`` accumulates a pixel-wise histogram of
``frame - pedestal`` residuals while maintaining a running per-pixel pedestal
estimate.

Use ``push_pedestal_no_update()`` to seed the pedestal estimate, then
``update_mean()`` before submitting frames with ``fill_async()``. Pending
asynchronous fills are drained by ``flush()``, and snapshot methods such as
``values()`` and ``pedestal_mean()`` return numpy arrays.

``fill_from_file()`` uses parallel file-reader workers and a double-buffered
pipeline. After the initial batch has been read, histogram processing of one
batch overlaps reading of the next. ``reader_threads`` and
``reader_chunk_size`` tune the I/O stage independently of the histogram worker
count. Two fixed-capacity buffers are allocated once and reused by alternating
their read and histogram roles. Their approximate memory use is ``2 *
reader_threads * reader_chunk_size * rows * cols * sizeof(uint16)``.

.. py:currentmodule:: aare

.. autoclass:: PedestalTrackingPixelHistogram
    :members:
    :undoc-members:
    :show-inheritance:
    :inherited-members:
