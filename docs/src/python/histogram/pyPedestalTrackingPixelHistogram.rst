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

**Saving and restoring the state**

``save_state()`` writes the complete state to a state file: the counts, the
pedestal, the noise as of the last ``update_mean()``, ``n_sigma`` and the
mask, including the frames that were still waiting to be filled.
``PedestalTrackingPixelHistogram.from_state_file(filename, n_threads=1,
max_pending=16)`` returns a new histogram that is filled further exactly as
the stored one would have been, with any number of threads, and
``hist.load_state(filename)`` replaces the state of an existing histogram
with the same image shape, number of bins and range. The state is exact for
a histogram whose pedestal has its 1000 frames, or has not been filled yet.
The file format is described in :doc:`../../StateFile`.

.. code:: python

    hist.save_state("hist.aare")

    hist = PedestalTrackingPixelHistogram.from_state_file("hist.aare", n_threads=8)
    other.load_state("hist.aare")

.. py:currentmodule:: aare

.. autoclass:: PedestalTrackingPixelHistogram
    :members:
    :undoc-members:
    :show-inheritance:
    :inherited-members:
