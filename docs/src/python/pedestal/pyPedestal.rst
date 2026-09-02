Pedestal
========

``Pedestal`` calculates a running mean and variance for each pixel in a series
of ``uint16`` frames. ``push()`` updates the cached mean immediately. For
faster batch initialization, use ``push_no_update()`` for each frame and call
``update_mean()`` after the batch.

Three specializations are available from :mod:`aare`:

* ``Pedestal_d`` uses ``float64`` storage
* ``Pedestal_f`` uses ``float32`` storage
* ``Pedestal_i16`` uses ``int16`` storage

Example
-------

.. code-block:: python

   from aare import Pedestal_d

   pedestal = Pedestal_d(512, 1024, 100)

   for frame in initialization_frames:
       pedestal.push_no_update(frame)

   pedestal.update_mean()
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
