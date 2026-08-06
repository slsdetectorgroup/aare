FastPedestal
============

``FastPedestal`` calculates a running mean, variance and standard deviation for each pixel in a
series of frames. The python binding only exposes ``uint16`` input but the underlying
C++ class is templated. Initialize it with ``n_samples`` frames using
``push_init()``. Once ``ready`` is true, use ``push()`` for steady-state
updates.

.. warning::
  
  FastPedestal is not usable until you have pushed ``n_samples`` initial frames with ``push_init(raw)``.
  You can check the state with ``ready``.

The public factory selects the bound C++ specialization from ``dtype``:

* ``numpy.float64`` creates ``FastPedestal_d``
* ``numpy.float32`` creates ``FastPedestal_f``
* ``numpy.int16`` creates ``FastPedestal_i16``

The internal calculations are done with double, but the cached mean and on demand var and std are returned in the specified type.

Factory
-------

.. py:currentmodule:: aare

.. autofunction:: FastPedestal

Loading from a file
-------------------

``FastPedestal.from_file()`` initializes the pedestal from ``n_samples``
frames after ``skip_first``, then applies steady-state updates for any frames
remaining in the file. The input frames must contain ``uint16`` data; ``dtype``
selects the output type of the pedestal statistics.

.. autofunction:: aare.FastPedestal.from_file

.. code-block:: python

   pedestal = FastPedestal.from_file(
       "frames.npy", n_samples=100, skip_first=10, dtype=np.float32
   )

Example
-------

.. code-block:: python

   import numpy as np
   from aare import FastPedestal

   pedestal = FastPedestal(512, 1024, n_samples=100, dtype=np.float32)

   # Initialize with n_samples frames
   for frame in initialization_frames:
       pedestal.push_init(frame)

   # Now we can push a frame for pedestal update
   if pedestal.ready:
       pedestal.push(next_frame)
       
   # Mean and std are  also ready
   mean = pedestal.mean()
   noise = pedestal.std()
   
   # Direct pedestal subtraction is also supported
   for frame in raw_data:
       image = frame - pedestal

Complete API
------------

The API below is for the ``float64`` specialization. All dtype variants share
the same API.

.. autoclass:: aare._aare.FastPedestal_d
   :special-members: __init__
   :members:
   :undoc-members:
   :show-inheritance:
   :inherited-members:
