ClusterFinder
=============

``ClusterFinder`` finds fixed-size photon clusters in single frames, using a
per-pixel pedestal and a noise threshold. It is a factory that returns the
compiled finder for the requested cluster size and dtype:

.. code-block:: python

   import numpy as np
   from aare import ClusterFinder

   finder = ClusterFinder((400, 400), cluster_size=(3, 3), dtype=np.int32)
   for frame in dark_frames:
       finder.push_pedestal_frame(frame)
   for frame_number, frame in enumerate(frames):
       finder.find_clusters(frame, frame_number)
   clusters = finder.steal_clusters()

.. autofunction:: aare.ClusterFinder

Saving and restoring the state
------------------------------

``save_state()`` writes the state of the finder to a state file: its
pedestal, ``nSigma`` and the threshold as it was last computed. A finder
that loads the file continues exactly where the stored one stopped, whether
its pedestal was still initializing or ready. A pedestal and threshold taken
once from a dark run can this way be reused for many data files or in other
processes.

Clusters that have been found but not yet taken with ``steal_clusters()``
are not stored, so take them before saving.

The file records the cluster size and the ``dtype`` of the finder that wrote
it. There are two ways to load it:

* ``ClusterFinder.load_state(filename, dtype=None, capacity=1024)`` returns
  a new finder. The image shape, the cluster size, ``nSigma`` and
  ``min_pedestal_samples`` come from the file, and so does the ``dtype``
  unless one is given.
* ``finder.load_state(filename)`` replaces the state of an existing finder
  and returns nothing. ``nSigma`` and ``min_pedestal_samples`` come from the
  file, the cluster size and image shape in the file must be those of the
  finder, and the finder keeps its ``dtype``. Clusters it still holds are
  dropped. If the file cannot be loaded, the finder is left unchanged.

A state can only be loaded into a finder with the same cluster size.

The file format is described in :doc:`../../StateFile`, including how to
read it with NumPy only. ``aare.read_state_header()`` returns its header as
a dict, for example to see the stored cluster size or ``n_sigma`` without
loading the arrays.

.. autofunction:: aare.ClusterFinder.load_state

.. code-block:: python

   finder = ClusterFinder((400, 400), cluster_size=(3, 3), dtype=np.int32)
   for frame in dark_frames:
       finder.push_pedestal_frame(frame)
   finder.save_state("finder.aare")

   # Later, or in another process: a new finder from the file, with the
   # cluster size stored in it and its dtype unless another is given
   finder = ClusterFinder.load_state("finder.aare")
   finder = ClusterFinder.load_state("finder.aare", dtype=np.float32)

   # Or into a finder that already exists, keeping its dtype
   finder = ClusterFinder((400, 400), cluster_size=(3, 3), dtype=np.float64)
   finder.load_state("finder.aare")

Using a pedestal taken elsewhere
--------------------------------

A finder is normally restored from its own state file, which includes the
pedestal. To give a finder only a pedestal, for example one taken from a
dark run with a :doc:`../pedestal/pyFastPedestal`, use
``finder.set_pedestal(pedestal)`` or, from the state file of that pedestal,
``finder.load_pedestal(filename)``. The pedestal can have any dtype and must
have the image shape of the finder.

Both keep ``nSigma`` and the clusters found so far, and
``min_pedestal_samples`` becomes ``n_samples`` of the pedestal. The
threshold follows the rule of the finder: it is recomputed as noise times
``nSigma`` if the pedestal is ready, otherwise when pedestal frames make it
ready. If the pedestal does not fit or the file cannot be loaded, the finder
is left unchanged.

.. code-block:: python

   dark = FastPedestal.from_file("dark_master_0.json")
   dark.save_state("dark.aare")

   finder = ClusterFinder((400, 400), cluster_size=(3, 3), n_sigma=5)
   finder.set_pedestal(dark)
   # or
   finder.load_pedestal("dark.aare")

``finder.get_pedestal()`` returns a copy of the pedestal of the finder as a
``FastPedestal``, and ``finder.save_pedestal(filename)`` writes it to a
state file that ``FastPedestal.load_state()`` and ``load_pedestal()`` read.

ClusterFinderMT
---------------

``ClusterFinderMT`` runs one cluster finder per thread. Pedestal frames go
to every thread, but each data frame goes to one thread, which updates its
pedestal from it, so the pedestals of the threads drift apart while clusters
are found. Its state file therefore holds the pedestal, ``nSigma`` and
threshold of every thread, and which thread takes the next frame.

The state and the pedestals can only be read or replaced while the finder is
stopped: ``stop()`` first lets the threads finish the frames they were
given, and the methods below raise ``RuntimeError`` on a running finder.

.. code-block:: python

   finder = ClusterFinderMT((400, 400), n_threads=3)
   collector = ClusterCollector(finder)
   # ... push pedestal frames, find clusters ...
   finder.stop()
   finder.save_state("finder.aare")

   # Later: a new finder with the stored number of threads, cluster size
   # and dtype, already started
   finder = ClusterFinderMT.load_state("finder.aare")

   # Or into a finder that already exists, with the same number of threads
   other.stop()
   other.load_state("finder.aare")
   other.start()

``finder.load_state()`` also takes the state file of a single-threaded
``ClusterFinder``. It holds one state, which is given to every thread or,
with ``thread_index``, to that thread only.

.. autofunction:: aare.ClusterFinderMT.load_state

The pedestal methods of ``ClusterFinder`` take a thread index here:

* ``finder.set_pedestal(pedestal, thread_index=None)`` and
  ``finder.load_pedestal(filename, thread_index=None)`` replace the pedestal
  of every thread or, with ``thread_index``, of that thread only. ``nSigma``
  is kept and the threshold of each thread that gets the pedestal follows
  the rule of the finder.
* ``finder.get_pedestal(thread_index=0)`` returns a copy of the pedestal of
  one thread as a ``FastPedestal``, and
  ``finder.save_pedestal(filename, thread_index=0)`` writes it to a state
  file.

.. code-block:: python

   finder.stop()
   finder.set_pedestal(dark)                  # every thread
   finder.set_pedestal(dark, thread_index=1)  # thread 1 only
   pedestals = [finder.get_pedestal(i) for i in range(finder.n_threads)]
   finder.start()
