VarClusterFinder
====================

Finds clusters of variable size by flood filling connected pixels above a
threshold. The seed and neighbour thresholds are either one fixed value or
per-pixel multiples of a noise map. Each cluster is reported as a ``Hit``
with its size, summed energy and maximum pixel; the coordinates and values
of every cluster pixel are stored in the flat arrays of ``Hits``. See the
Python page for a description of the algorithm and the output.

Saving and restoring the configuration
--------------------------------------

A ``VarClusterFinder`` accumulates nothing between frames except its hits,
so its state is its configuration: the thresholds, the factors, the
connectivity, ``erase_clusters``, the noise map and the mask. ``state()``
returns it as a plain ``VarClusterFinderState``, and ``set_state()`` or the
constructor that takes a state restore it. The state must have the image
shape of the finder that takes it, which may have another pixel type. Hits
that have not been taken with ``steal_hits()`` are not part of the state,
and ``set_state()`` drops those the finder holds. The state is kept in a
state file with ``save_state()`` and ``load_state()``, see :doc:`StateFile`:

.. code-block:: c++

   #include "aare/StateFile.hpp"

   aare::save_state(finder.state(), "finder.aare");

   aare::VarClusterFinder<double> restored(
       aare::load_state<aare::VarClusterFinderState>("finder.aare"));

.. doxygenstruct:: aare::VarClusterFinderState
   :members:

API
---

.. doxygenclass:: aare::VarClusterFinder
   :members:
   :undoc-members:
