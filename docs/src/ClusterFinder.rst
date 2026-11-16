ClusterFinder
=============

Saving and restoring the state
------------------------------

``state()`` returns what a cluster finder has accumulated as a plain
``ClusterFinderState``: the state of its pedestal, ``nSigma`` and the
threshold. A finder constructed from that state, or given it with
``set_state()``, continues exactly as the original would, whether its
pedestal was still initializing or already ready.

The threshold is part of the state because it cannot be derived from the
rest: it is the noise multiplied by ``nSigma`` at the time of the last
``update_threshold()``, while ``find_clusters()`` keeps updating the
pedestal.

A state can only be given to a finder with the same cluster size and image
shape. The value type of the clusters, the frame type and the pedestal type
may differ: the state holds ``n_sigma`` and the threshold as double and the
pedestal as a :doc:`FastPedestal` state, and only records the cluster and
pedestal types of the finder it came from in ``cluster_dtype`` and
``pedestal.dtype``.

Clusters that were found but not yet taken with ``steal_clusters()`` are not
part of the state, and ``set_state()`` drops those the finder still holds.
Take them first if they are needed.

.. code-block:: c++

   aare::ClusterFinder<aare::Cluster<int32_t, 3, 3>> finder({400, 400});
   // ... push pedestal frames, find clusters ...
   auto clusters = finder.steal_clusters();
   aare::ClusterFinderState state = finder.state();

   // Later, possibly in another process once the state has been stored
   aare::ClusterFinder<aare::Cluster<int32_t, 3, 3>> restored(state);

   // Or replace the state of an existing finder with the same cluster
   // size and image shape
   other.set_state(state);

``set_state()`` throws if the cluster size, the shapes or the pedestal state
do not fit, and then leaves the finder unchanged.

The state itself is plain data and not tied to a file format. To keep it,
write it to a state file with ``save_state()`` and read it back with
``load_state()``, see :doc:`StateFile`:

.. code-block:: c++

   #include "aare/StateFile.hpp"

   aare::save_state(finder.state(), "finder.aare");

   aare::ClusterFinder<aare::Cluster<int32_t, 3, 3>> restored(
       aare::load_state<aare::ClusterFinderState>("finder.aare"));

.. doxygenstruct:: aare::ClusterFinderState
   :members:

API
---

.. doxygenclass:: aare::ClusterFinder
   :members:
   :undoc-members:
