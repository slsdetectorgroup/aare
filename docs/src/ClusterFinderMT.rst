ClusterFinderMT
==================

Saving and restoring the state
------------------------------

Each thread of a ``ClusterFinderMT`` has its own cluster finder. Pedestal
frames go to every thread, but each data frame goes to one thread, which
updates its pedestal from it, so the pedestals of the threads drift apart
while clusters are found. ``state()`` therefore returns a
``ClusterFinderMTState`` with one ``ClusterFinderState`` per thread, see
:doc:`ClusterFinder`, and the thread that takes the next frame. A finder
constructed from that state, or given it with ``set_state()``, continues as
the original would: every thread with its own pedestal and threshold, and
every further frame going to the thread that would have had it.

The cluster finders belong to their threads while those run. ``state()``,
``set_state()``, ``set_pedestal()`` and ``get_pedestal()`` throw unless the
finder is stopped; ``stop()`` first lets the threads finish the frames they
were given. Clusters that have not been taken from the sink are not part of
the state.

.. code-block:: c++

   finder.stop();
   aare::ClusterFinderMTState state = finder.state();
   aare::save_state(state, "finder.aare");

   // Later: a new finder with the stored number of threads, started
   aare::ClusterFinderMT<aare::Cluster<int32_t, 3, 3>> restored(
       aare::load_state<aare::ClusterFinderMTState>("finder.aare"));

   // Or into an existing finder with the same number of threads
   other.stop();
   other.set_state(state);
   other.start();

``set_state()`` with a ``ClusterFinderMTState`` needs a state for the number
of threads of the finder and leaves the finder unchanged if any part of the
state does not fit. To start all threads from one state, for example that of
a single-threaded ``ClusterFinder``, pass a ``ClusterFinderState``; with a
thread index it goes to that thread only.

Pedestals
---------

``set_pedestal()`` replaces the pedestal of every thread, or of one thread
if a thread index is given, with a :doc:`FastPedestal` or its state. As for
``ClusterFinder::set_pedestal()``, ``nSigma`` is kept and the threshold of
each thread that gets the pedestal is recomputed if the pedestal is ready.
``get_pedestal(thread_index)`` returns a copy of the pedestal of one thread.

.. code-block:: c++

   finder.stop();
   finder.set_pedestal(dark);                 // every thread
   finder.set_pedestal(dark, 1);              // thread 1 only
   auto pedestal = finder.get_pedestal(2);    // FastPedestal of thread 2
   finder.start();

.. doxygenstruct:: aare::ClusterFinderMTState
   :members:

.. doxygenclass:: aare::ClusterFinderMT
   :members:
   :undoc-members: