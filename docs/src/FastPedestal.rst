FastPedestal
=============

``FastPedestal`` keeps a per-pixel running mean and standard deviation and is
the pedestal used by :doc:`ClusterFinder`. The Python interface is described
in :doc:`python/pedestal/pyFastPedestal`.

Saving and restoring the state
------------------------------

``state()`` returns everything a pedestal has accumulated as a plain
``FastPedestalState``. A pedestal constructed from that state, or given it
with ``set_state()``, continues exactly as the original would, whether it was
still initializing or already ready. The state is independent of the
``PEDESTAL_TYPE``: the moments are always ``double`` and the cached mean is
recomputed, so a state taken from a ``FastPedestal<double>`` can be given to
a ``FastPedestal<float>``. Its ``dtype`` only records the ``PEDESTAL_TYPE``
it was taken from, for whoever restores it and wants the same kind of
pedestal.

.. code-block:: c++

   aare::FastPedestal<double> pedestal(rows, cols, 1000);
   // ... add_init_frame() and push_ema() ...

   aare::FastPedestalState state = pedestal.state();

   // Later, possibly in another process once the state has been stored
   aare::FastPedestal<double> restored(state);

   // Or replace the state of an existing pedestal of the same shape,
   // including its n_samples
   aare::FastPedestal<float> other(rows, cols);
   other.set_state(state);

``set_state()`` throws if the shape or the sample counts of the state are not
valid and then leaves the pedestal unchanged.

The state itself is plain data and not tied to a file format. To keep it,
write it to a state file with ``save_state()`` and read it back with
``load_state()``, see :doc:`StateFile`:

.. code-block:: c++

   #include "aare/StateFile.hpp"

   aare::save_state(pedestal.state(), "dark.aare");

   aare::FastPedestal<double> restored(
       aare::load_state<aare::FastPedestalState>("dark.aare"));

.. doxygenstruct:: aare::FastPedestalState
   :members:
   :undoc-members:

.. doxygenclass:: aare::FastPedestal
   :members:
   :undoc-members:
