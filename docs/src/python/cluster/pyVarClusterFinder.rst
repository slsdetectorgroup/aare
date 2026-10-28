VarClusterFinder
===================

Finds clusters of variable size by flood filling connected pixels above a
threshold. Use it when the cluster shape is not known in advance, for
example for charge spread over more than 3x3 pixels or for tracks. For
fixed size photon clusters ``ClusterFinder`` is the faster choice.

The image is scanned row by row. A pixel above the seed threshold starts a
cluster, which then grows into every 4 or 8 connected neighbour
(``connectivity``, default 8) above the neighbour threshold. Without a noise map the thresholds are fixed values:
``threshold``, given to the constructor, for seeds and
``neighbour_threshold`` for neighbours, which follows ``threshold`` until
it is set and again after assigning ``None``. With ``noise_map`` set the
thresholds are per pixel: ``seed_factor`` times the noise for seeds and
``neighbour_factor`` times the noise for neighbours, both 5 by default.
Assign ``None`` to ``noise_map`` to return to the fixed thresholds.

**Usage**

.. code:: python

    import numpy as np
    from aare import VarClusterFinder

    cf = VarClusterFinder((512, 1024), threshold=50.0)
    for frame in frames:            # float64, pedestal subtracted
        cf.find_clusters(frame)
    hits = cf.steal_hits()          # VarClusterHits
    energies = hits.hits["energy"]  # one record per cluster
    rows, cols, values = hits.pixels(0)  # pixels of the first cluster

    # Grow clusters into pixels above a lower threshold than the seeds
    cf.neighbour_threshold = 20.0

    # Thresholds in units of the per pixel noise instead of fixed values
    cf.noise_map = noise            # same shape as the frames, copied
    cf.seed_factor = 5
    cf.neighbour_factor = 2

    cf.mask = bad_pixels            # bool array, True on excluded pixels

**Hits**

``steal_hits`` returns all clusters found since the last call as a
``VarClusterHits`` with four NumPy arrays that are views into it:

- ``hits``: structured array with one record per cluster: ``size``, the
  number of pixels (int32); ``row``, ``col``, the position of the maximum
  pixel (int16); ``energy``, the sum over all pixels (float64); ``max``,
  the value of the maximum pixel (float64); ``offset``, the index of the
  cluster's first pixel in the arrays below (int64).
- ``rows``, ``cols`` (int16) and ``values`` (float64): one entry per
  cluster pixel. The pixels of ``hits[i]`` are the slice
  ``offset:offset + size``, in fill order starting with the seed, and
  ``pixels(i)`` returns the three slices.

Every pixel of every cluster is stored, whatever its size. The layout
suits vectorised per cluster sums, for example
``np.add.reduceat(hits.values, hits.hits["offset"])`` for the energies,
and it maps directly onto ragged array libraries such as Awkward. Hits are
returned in the order in which their seeds were found, row by row, and
accumulate across frames until ``steal_hits`` or ``clear_hits`` is called.

``labeled`` is the label image of the last frame, returned as a copy: 0
outside clusters and the 1-based index of the cluster within that frame
otherwise.

**Mask**

``mask`` is a boolean array of the image shape, ``True`` on the pixels to
exclude, or ``None``. A masked pixel never seeds a cluster and is never
added to one, so clusters grow around it as they do around the image
border, and it is never modified. Masked pixels are -1 in ``labeled``.
The array is copied on assignment; assign ``None`` to remove the mask.

Clusters next to a masked pixel or to the image border have lost part of
their charge. The finder keeps them; to find them afterwards, dilate the
mask by one pixel, treat the border as masked and reduce per cluster:

.. code:: python

    pad = np.pad(mask, 1, constant_values=True)
    near = np.zeros_like(mask)
    for dr in (0, 1, 2):
        for dc in (0, 1, 2):
            near |= pad[dr : dr + mask.shape[0], dc : dc + mask.shape[1]]
    touches = np.logical_or.reduceat(near[hits.rows, hits.cols], hits.hits["offset"])
    clean = hits.hits[~touches]

**Input image**

``find_clusters`` takes a C-contiguous float64 array of the shape given to
the constructor and leaves it untouched. With ``erase_clusters = True`` the
clusters are erased from the frame as the fill visits them: every cluster
pixel is set to zero, and so are the sub-threshold neighbours around it,
because charge that leaked below threshold would otherwise bias a pedestal
updated from the erased frame. A zeroed pixel compares as zero for the rest
of the frame, which only matters for negative thresholds.

.. py:currentmodule:: aare

.. autoclass:: VarClusterFinder
    :members:
    :undoc-members:
    :show-inheritance:
    :inherited-members:

.. autoclass:: VarClusterHits
    :members:
    :undoc-members:
