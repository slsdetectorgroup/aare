VarClusterFinder
====================

Finds clusters of variable size by flood filling connected pixels above a
threshold. The seed and neighbour thresholds are either one fixed value or
per-pixel multiples of a noise map. Each cluster is reported as a ``Hit``
with its size, summed energy, maximum pixel and the first
``MAX_CLUSTER_SIZE`` pixels. See the Python page for a description of the
algorithm and the output.

.. doxygenclass:: aare::VarClusterFinder
   :members:
   :undoc-members:
