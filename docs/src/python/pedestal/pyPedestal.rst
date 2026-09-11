Pedestal
========

``Pedestal`` calculates a running mean and variance for each pixel in a series
of ``uint16`` frames. ``push()`` updates the cached mean immediately.

Internal sums and sums of squares always use ``float64``. Three
specializations are available from :mod:`aare` for the mean, variance, and
standard deviation output types:

* ``Pedestal_d`` returns ``float64``
* ``Pedestal_f`` returns ``float32``
* ``Pedestal_i16`` returns ``int16``

The public ``Pedestal`` factory selects the specialization from ``dtype``,
defaulting to ``numpy.float64``.

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
