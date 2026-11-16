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
