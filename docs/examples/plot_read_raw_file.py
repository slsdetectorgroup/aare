# SPDX-License-Identifier: MPL-2.0
"""
Read a raw file
===============

Read frames from a raw file written by the slsReceiver with ``aare.RawFile``.
The file used here is from a Jungfrau 500k module using two UDP
interfaces. See :doc:`/python/file/pyRawFile` for the full API,
including reading a region of interest and inserting gap pixels.

.. note::

    The file read here is generated when the documentation is built.
    The code is in ``docs/examples/example_data.py``.
"""

import matplotlib.pyplot as plt
import numpy as np

from aare import RawFile, add_colorbar

# sphinx_gallery_start_ignore
from example_data import write_jungfrau_raw_file

write_jungfrau_raw_file("run")
# sphinx_gallery_end_ignore

# %%
# .. rubric:: Open the file
#
# A raw file consists of a master file with the metadata of the measurement
# and one or more data files with the frames. In this case each UDP
# interface sends half of the module, and the receiver writes one data file
# for each of them::
#
#     run_master_0.json
#     run_d0_f0_0.raw
#     run_d1_f0_0.raw
#
# Open the master file. ``RawFile`` finds the data files from it and
# assembles the parts into full frames.

fname = "run_master_0.json"

with RawFile(fname) as f:
    print(f"{f.total_frames = }")
    print(f"{f.rows() = }")
    print(f"{f.cols() = }")
    print(f"{f.bitdepth = }")

# %%
# The metadata from the master file is available as ``f.master``. See
# :doc:`/python/file/pyRawMasterFile` for all the fields.

with RawFile(fname) as f:
    print(f"{f.master.detector_type = }")
    print(f"{f.master.version = }")
    print(f"{f.master.exptime = }")
    print(f"{f.master.period = }")
    print(f"{f.master.udp_interfaces_per_module = }")

# %%
# .. rubric:: Read a frame
#
# ``read_frame`` returns the header and the data of the next frame. The data
# is a NumPy array with the shape ``(rows, columns)``.

with RawFile(fname) as f:
    header, data = f.read_frame()

print(f"{data.shape = }")
print(f"{data.dtype = }")

# %%
# The header holds what the detector sent along with the frame, for example
# the frame number and the timestamp. There is one header for each data
# file, so here it has two entries.

print(f"{header.shape = }")
print(f"{header['frameNumber'] = }")
print(f"{header['timestamp'] = }")
print(header.dtype.names)

# %%

fig, ax = plt.subplots(figsize=(8, 4))
im = ax.imshow(data)
add_colorbar(ax, im)
ax.set_xlabel("column")
ax.set_ylabel("row")
ax.set_title("Raw ADU")
plt.show()

# %%
# .. rubric:: Read several frames
#
# ``read_n`` reads the next frames into one array with the shape
# ``(frames, rows, columns)``. The file keeps track of its position:
# ``tell`` returns the index of the next frame and ``seek`` moves to a
# frame. Indices start at 0.

with RawFile(fname) as f:
    header, frames = f.read_n(5)
    print(f"{frames.shape = }")
    print(f"{f.tell() = }")

    f.seek(10)
    header, data = f.read_frame()
    print(f"{header['frameNumber'] = }")

# %%
# ``read_frame`` also takes the index of the frame to read.

with RawFile(fname) as f:
    header, data = f.read_frame(3)

print(f"{header['frameNumber'] = }")

# %%
# .. rubric:: Process the whole file
#
# A measurement usually doesn't fit in memory, so we iterate over the file and reduce
# each frame to what is needed.
#
# Here we average the ADC value of each pixel over the frames in which the
# pixel was in G0. The values of the other gain stages are on a different
# scale and cannot be averaged together with them.

with RawFile(fname) as f:
    total = np.zeros((f.rows(), f.cols()))
    n_g0 = np.zeros((f.rows(), f.cols()), dtype=np.int64)

    for header, data in f:
        in_g0 = (data >> 14) == 0
        total += np.where(in_g0, data & 0x3FFF, 0)
        n_g0 += in_g0

# Pixels that were never in G0 are left empty
mean_g0 = np.full(total.shape, np.nan)
np.divide(total, n_g0, out=mean_g0, where=n_g0 > 0)

print(f"{np.isnan(mean_g0).sum()} pixels were never in G0")

# %%
# The weak rings are easier to see in the average than in the single frame.
# The pixels in the middle of the brightest ring switched gain in every
# frame and are left white. The bright spots where a ring crosses the border
# between two chips are pixels of double size, which collect twice as many
# photons.

fig, ax = plt.subplots(figsize=(8, 4))
im = ax.imshow(mean_g0, vmin=2400, vmax=3500)
add_colorbar(ax, im)
ax.set_xlabel("column")
ax.set_ylabel("row")
ax.set_title("mean ADC value in G0")
plt.show()

# %%
# The ADC values still include the pedestal of each pixel. Converting them
# to energy takes a pedestal and a gain calibration, see
# :doc:`/pycalibration`.

# sphinx_gallery_start_ignore
import glob
import os

for name in glob.glob("run_*"):
    os.remove(name)
# sphinx_gallery_end_ignore
