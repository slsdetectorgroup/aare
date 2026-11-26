# SPDX-License-Identifier: MPL-2.0
"""
Read a cluster file
===================

Read clusters from a file with ``aare.ClusterFile`` and build a hit
map and an energy spectrum. See :doc:`/python/file/pyClusterFile`
for the full interface, including filtering by region of interest and noise.

.. note::

    The file read here is generated when the documentation is built. Frames
    with a beam spot of 600 ADU photons are simulated and passed through
    ``aare.ClusterFinderMT``, which writes the clusters it finds to
    ``clusters.clust``. The code is in ``docs/examples/example_data.py``.
"""

import boost_histogram as bh
import matplotlib.pyplot as plt
import numpy as np

from aare import ClusterFile, add_colorbar, hitmap

# sphinx_gallery_start_ignore
from example_data import write_cluster_file

write_cluster_file("clusters.clust")
# sphinx_gallery_end_ignore

# %%
# .. rubric:: Open the file
#
# A cluster file stores neither the size of the clusters nor the data type of
# their pixels, so both have to be given when opening it and must match what
# the file was written with. Here the clusters are 3 x 3 pixels of ``int32``
# which also happens to be the default.

fname = "clusters.clust"

with ClusterFile(fname, cluster_size=(3, 3), dtype=np.int32) as f:
    clusters = f.read_frame()

print(f"{clusters.frame_number = }")
print(f"{clusters.size = }")

# %%
# ``read_frame`` returns the clusters of the next frame as a
# ``ClusterVector``. View it as a NumPy array to get to the data: ``x`` and
# ``y`` are the column and row of the central pixel and ``data`` holds the
# pixel values of each cluster.

arr = np.array(clusters, copy=False)

print(arr.dtype)
print(arr[0])

# %%
# .. rubric:: Iterate over frames or chunks
#
# There are two ways to loop over a file. ``frames()`` yields one
# ``ClusterVector`` per frame, also for frames without clusters. Use it when
# the frame number matters, for example to follow the number of photons over
# time. Here we stop after the first five frames.

with ClusterFile(fname, cluster_size=(3, 3), dtype=np.int32) as f:
    for frame in f.frames():
        if frame.frame_number > 5:
            break
        print(f"frame {frame.frame_number}: {frame.size} clusters")

# %%
# ``chunks()`` yields a fixed number of clusters at a time, regardless of
# which frame they belong to, and the last chunk holds what is left. Use it
# when only the clusters matter. A chunk can span several frames, so its
# ``frame_number`` is not meaningful.

with ClusterFile(fname, cluster_size=(3, 3), dtype=np.int32) as f:
    for clusters in f.chunks(10_000):
        print(f"{clusters.size} clusters")

# %%
# Iterating over the file itself is the same as ``chunks()`` with the
# ``chunk_size`` given when opening the file.
#
# .. rubric:: Process the whole file
#
# A measurement rarely fits in memory, so read the file in chunks
# and reduce each chunk to what is needed.
#
# Here we fill a hit map with the positions of the clusters and two spectra: one of
# the sum of all nine pixels and one of the central pixel alone. The spectra
# are `boost-histogram <https://boost-histogram.readthedocs.io>`_ histograms,
# which can be filled chunk by chunk.

image_size = (200, 200)  # rows, columns of the detector

image = np.zeros(image_size, dtype=np.int64)
spectrum_sum = bh.Histogram(bh.axis.Regular(125, 0, 1000))
spectrum_center = bh.Histogram(bh.axis.Regular(125, 0, 1000))
n_clusters = 0

with ClusterFile(fname, cluster_size=(3, 3), dtype=np.int32, chunk_size=5000) as f:
    # Iterating directly over the file returns chunks of chunk_size clusters
    for clusters in f:
        image += hitmap(image_size, clusters)
        spectrum_sum.fill(clusters.sum())

        # View the data as a NumPy array to access the central pixel
        arr = np.array(clusters, copy=False)
        spectrum_center.fill(arr["data"][:, 1, 1])

        n_clusters += clusters.size

print(f"{n_clusters = }")

# %%
# .. rubric:: Plot the result
#
# The hit map shows where the photons hit the detector.

fig, ax = plt.subplots()
im = ax.imshow(image)
add_colorbar(ax, im)
ax.set_xlabel("column")
ax.set_ylabel("row")
ax.set_title("hits per pixel")
plt.show()

# %%
# The sum of the cluster has a peak at the energy of the photons. The central
# pixel alone holds less: a photon that lands near the border of a pixel
# shares its charge with the neighbouring pixels, which gives the tail
# towards lower values.

fig, ax = plt.subplots()
ax.stairs(spectrum_sum.values(), spectrum_sum.axes[0].edges, label="3x3 sum")
ax.stairs(
    spectrum_center.values(), spectrum_center.axes[0].edges, label="central pixel"
)
ax.set_xlabel("energy [ADU]")
ax.set_ylabel("clusters")
ax.grid()
ax.legend()
plt.show()

# sphinx_gallery_start_ignore
import os

os.remove(fname)
# sphinx_gallery_end_ignore
