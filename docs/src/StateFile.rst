State files
===========

A state file stores everything an object has accumulated, so that it can be
restored later, possibly in another process, and continue exactly where it
stopped. ``FastPedestal``, ``ClusterFinder``, ``ClusterFinderMT`` and
``VarClusterFinder`` have a state that can be stored this way.

.. code-block:: c++

   #include "aare/StateFile.hpp"

   aare::save_state(pedestal.state(), "dark.aare");

   aare::FastPedestal<double> restored(
       aare::load_state<aare::FastPedestalState>("dark.aare"));

A cluster finder is stored the same way, with ``finder.state()`` and
``load_state<aare::ClusterFinderState>()``, see :doc:`ClusterFinder`.

In Python the same file is written with ``pedestal.save_state()`` and read
with ``FastPedestal.load_state()``, which returns a new pedestal, or
``pedestal.load_state()``, which loads into an existing one, see
:doc:`python/pedestal/pyFastPedestal`. ``ClusterFinder`` has the same three
functions, see :doc:`python/cluster/pyClusterFinder`. The file extension is
free; the examples use ``.aare``.

To see what a file holds without reading its arrays, ``read_state_header()``
returns the JSON header, as text in C++ and as a dict in Python:

.. code-block:: python

   >>> aare.read_state_header("dark.aare")["state"]["dtype"]
   '<f4'

``save_state()`` writes to a temporary file next to the target, named as the
target with ``.tmp`` appended, and renames it when it is complete. A crash
while writing therefore never leaves a partly written state file, and an
existing file is replaced only by a complete one. Saves to the same target
must not run at the same time.

File layout
-----------

A state file is a JSON header followed by the arrays of the state as raw
data:

.. code-block:: text

   bytes 0-7    magic "AARESTAT"
   bytes 8-15   length N of the header, uint64 little-endian
   N bytes      JSON header, UTF-8, padded with spaces
   rest         arrays, little-endian, C order

The header is padded so that the data section, which starts at byte
``16 + N``, begins on a multiple of 64 bytes. Every array also starts on a
multiple of 64 bytes within the data section, so the arrays can be memory
mapped.

The header of a ``FastPedestal<float>`` with 400 x 400 pixels:

.. code-block:: json

   {
     "format_version": 1,
     "aare_version": "2026.9.30",
     "type": "FastPedestal",
     "state": {
       "version": 1,
       "dtype": "<f4",
       "n_samples": 1000,
       "cur_samples": 1000,
       "sum": {"dtype": "<f8", "shape": [400, 400], "offset": 0},
       "sum2": {"dtype": "<f8", "shape": [400, 400], "offset": 1280000}
     }
   }

``format_version``
   Version of the layout and of the header fields outside ``state``. A
   reader rejects a file with a newer version than it knows.

``aare_version``
   Version of aare that wrote the file. For information only.

``type``
   Class whose state the file holds. Loading a file as another type is an
   error.

``state``
   The state itself. Its ``version`` is that of the fields of this type of
   state; a reader rejects a newer one than it knows. Scalars are JSON
   values. An array is an object with exactly the keys ``dtype`` (a NumPy
   dtype string, ``"|b1"`` for a mask of ``bool``), ``shape`` and ``offset``
   (in bytes, relative to the start of the data section), placed where the
   array belongs in the state. A field that a state can leave out is absent
   or ``null``. The fields of a ``FastPedestal`` are
   those of ``FastPedestalState``, see :doc:`FastPedestal`: its ``dtype`` is
   the type of the mean and standard deviation of the pedestal that wrote the
   file, a NumPy dtype string like the arrays'. A reader may create a
   pedestal of that type or of another; the moments are double either way.

A state can contain another state. The header of a 3 x 3 ``ClusterFinder``
with ``int32`` clusters holds the state of its pedestal under ``pedestal``,
with the fields that a ``FastPedestal`` file has under ``state``,
including its own ``version``. Several states of one type are stored as a
list of such objects:

.. code-block:: json

   {
     "format_version": 1,
     "aare_version": "2026.9.30",
     "type": "ClusterFinder",
     "state": {
       "version": 1,
       "cluster_dtype": "<i4",
       "cluster_size_x": 3,
       "cluster_size_y": 3,
       "n_sigma": 5.0,
       "threshold": {"dtype": "<f8", "shape": [400, 400], "offset": 0},
       "pedestal": {
         "version": 1,
         "dtype": "<f8",
         "n_samples": 1000,
         "cur_samples": 1000,
         "sum": {"dtype": "<f8", "shape": [400, 400], "offset": 1280000},
         "sum2": {"dtype": "<f8", "shape": [400, 400], "offset": 2560000}
       }
     }
   }

The fields are those of ``ClusterFinderState``, see :doc:`ClusterFinder`.
``cluster_size_x`` and ``cluster_size_y`` are the number of columns and rows
of a cluster; the state can only be loaded into a finder with that cluster
size. ``cluster_dtype`` is the value type of the clusters of the finder that
wrote the file; a reader may create a finder of that type or of another.
``threshold`` is the per-pixel threshold as it was last computed, which is
in general not ``n_sigma`` times the noise that follows from ``pedestal``.

A ``ClusterFinderMT`` has one cluster finder per thread, whose pedestals
differ. Its header holds their states as a list under ``finders``, each with
the fields that a ``ClusterFinder`` file has under ``state``, and the index
of the thread that takes the next frame:

.. code-block:: json

   {
     "format_version": 1,
     "aare_version": "2026.9.30",
     "type": "ClusterFinderMT",
     "state": {
       "version": 1,
       "next_thread": 2,
       "finders": [
         {"version": 1, "cluster_dtype": "<i4", "...": "..."},
         {"version": 1, "cluster_dtype": "<i4", "...": "..."},
         {"version": 1, "cluster_dtype": "<i4", "...": "..."}
       ]
     }
   }

A ``VarClusterFinder`` accumulates nothing between frames, so its state is
its configuration, with the fields of ``VarClusterFinderState``, see
:doc:`VarClusterFinder`. ``neighbour_threshold``, ``noise_map`` and ``mask``
are left out when the finder has none:

.. code-block:: json

   {
     "format_version": 1,
     "aare_version": "2026.9.30",
     "type": "VarClusterFinder",
     "state": {
       "version": 1,
       "dtype": "<f8",
       "rows": 400,
       "cols": 400,
       "threshold": 50.0,
       "neighbour_threshold": 20.0,
       "seed_factor": 5.0,
       "neighbour_factor": 2.0,
       "connectivity": 8,
       "erase_clusters": false,
       "noise_map": {"dtype": "<f8", "shape": [400, 400], "offset": 0},
       "mask": {"dtype": "|b1", "shape": [400, 400], "offset": 1280000}
     }
   }

A reader ignores fields it does not know, and reads a field that was added
after the first files were written only if it is there. Later versions can
therefore add fields without a new version, and both older and newer
versions of aare read the file. Removing a field or changing its meaning
requires a new ``version`` of that state, which leaves the files of the
other types readable; ``format_version`` changes only with the layout or the
header fields outside ``state``.

Reading a state file with NumPy
-------------------------------

Because the header describes every array, a state file can be read without
aare. This function returns the header with every array description replaced
by the array:

.. code-block:: python

   import json
   import numpy as np

   def read_state_file(fname):
       with open(fname, "rb") as f:
           if f.read(8) != b"AARESTAT":
               raise ValueError("not an aare state file")
           header_length = int.from_bytes(f.read(8), "little")
           header = json.loads(f.read(header_length))
       data_start = 16 + header_length

       def resolve(node):
           if isinstance(node, dict):
               if node.keys() == {"dtype", "shape", "offset"}:
                   return np.fromfile(
                       fname,
                       dtype=node["dtype"],
                       count=int(np.prod(node["shape"])),
                       offset=data_start + node["offset"],
                   ).reshape(node["shape"])
               return {key: resolve(value) for key, value in node.items()}
           if isinstance(node, list):
               return [resolve(value) for value in node]
           return node

       return resolve(header)

   state = read_state_file("dark.aare")["state"]
   mean = state["sum"] / state["n_samples"]  # once cur_samples == n_samples

API
---

.. doxygenfunction:: aare::save_state

.. doxygenfunction:: aare::load_state

.. doxygenfunction:: aare::read_state_header
