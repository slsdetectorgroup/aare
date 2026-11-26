Pedestal
=============

Saving and restoring the state
------------------------------

``state()`` returns everything the pedestal has accumulated as a plain
``PedestalState``, and ``set_state()`` or the constructor that takes a state
restore it, so that a pedestal continues exactly where it stopped. A
pedestal of any ``SUM_TYPE`` can take the state. It is kept in a state file
with ``save_state()`` and ``load_state()``, see :doc:`StateFile`:

.. code-block:: c++

   #include "aare/StateFile.hpp"

   aare::save_state(pedestal.state(), "dark.aare");

   aare::Pedestal<double> restored(
       aare::load_state<aare::PedestalState>("dark.aare"));

.. doxygenstruct:: aare::PedestalState
   :members:

API
---

.. doxygenclass:: aare::Pedestal
   :members:
   :undoc-members:
   :private-members: