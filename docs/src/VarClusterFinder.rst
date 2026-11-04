VarClusterFinder
====================

Finds clusters of variable size by flood filling connected pixels above a
threshold. The seed and neighbour thresholds are either one fixed value or
per-pixel multiples of a noise map. Each cluster is reported as a ``Hit``
with its size, summed energy and maximum pixel; the coordinates and values
of every cluster pixel are stored in the flat arrays of ``Hits``. See the
Python page for a description of the algorithm and the output.

.. doxygenclass:: aare::VarClusterFinder
   :members:
   :undoc-members:
