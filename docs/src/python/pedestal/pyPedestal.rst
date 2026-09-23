Pedestal
========

``Pedestal`` calculates a running mean and population standard deviation for
each pixel in a series of ``uint16`` frames. ``push()`` updates the cached mean
immediately; ``std()`` calculates the noise from the current statistics.

``push()`` and ``push_with_threshold()`` require C-contiguous, two-dimensional
NumPy frames with dtype ``uint16`` and shape matching the pedestal.
``push_with_threshold()`` also requires a C-contiguous, two-dimensional
threshold array with the pedestal's output dtype and shape. Noncontiguous
inputs raise ``TypeError``; inputs with the wrong number of dimensions raise
``ValueError``. Mismatched frame or threshold shapes raise ``RuntimeError``.
Use ``numpy.ascontiguousarray()`` to copy a sliced or transposed array into the
required layout when needed.

Internal sums and sums of squares always use ``float64``. Three
specializations are available from :mod:`aare` for the mean and standard
deviation output types:

* ``Pedestal_d`` returns ``float64``
* ``Pedestal_f`` returns ``float32``
* ``Pedestal_i16`` returns ``int16``

The public ``Pedestal`` factory selects the specialization from ``dtype``,
defaulting to ``numpy.float64``.

Constructor dimensions and ``n_samples`` must be positive integers. Negative
values raise ``TypeError`` and zero values raise ``RuntimeError``.
Internally, negative variance caused by floating-point roundoff is clamped to
zero before taking its square root, keeping the standard deviation finite for
nearly constant inputs. Only the final standard deviation is converted to the
output dtype.

Factory
-------

.. py:currentmodule:: aare

.. autofunction:: Pedestal

Example
-------

.. code-block:: python

   import numpy as np
   from aare import Pedestal

   pedestal = Pedestal(512, 1024, n_samples=100, dtype=np.float32)

   for frame in initialization_frames:
       pedestal.push(frame)

   mean = pedestal.mean()
   noise = pedestal.std()

Complete API
------------

The API below is for the ``float64`` specialization. All dtype variants share
the same API.

.. autoclass:: aare._aare.Pedestal_d
   :special-members: __init__
   :members:
   :undoc-members:
   :show-inheritance:
   :inherited-members:
