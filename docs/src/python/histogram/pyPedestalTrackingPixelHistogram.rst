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

.. py:currentmodule:: aare

.. autoclass:: PedestalTrackingPixelHistogram
    :members:
    :undoc-members:
    :show-inheritance:
    :inherited-members:
