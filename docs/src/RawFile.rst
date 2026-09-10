RawFile
===============

``total_frames()`` is the minimum actual frame count across all selected
``RawSubFile`` objects in all ROIs. Each subfile count includes its complete
series of ``.raw`` files and is determined when the file is opened. Disabled
UDP ports are excluded. An empty subfile makes the count zero; opening a file
with no selected subfiles raises an error.

A warning is logged once on opening if the subfile counts differ or their
minimum differs from ``master().frames_in_file()``. It reports the subfile count
range, the master count, and the count used for reading. The master metadata
is preserved. Its expected frame count does not affect the count used for
reading.

Frame, batch, ROI, and frame-number reads use this minimum as their bound.
Frame-number synchronization may still fail within this bound if detector
frame numbers differ between subfiles.

Read errors include the attempted zero-based frame index and the failing file
path. Errors from a data subfile identify that subfile's path and preserve
the underlying failure reason without adding a master-file wrapper or C++
source location. Out-of-range subfile errors also report the available frame
count across that subfile's series. This applies to frame, batch, ROI, and
frame-number reads, including reads through ``aare::File``.

The frame index is the position in the file, not the frame number recorded by
the detector. Batch errors report the index of the frame that failed.

Synchronization errors identify the last ``.raw`` data file read for the module
that ran out of frames. Requests rejected as out of range before accessing a
subfile report the master path, since there is no single failing data file.
The top-level frame bounds error states the number of frames in the file and
that indices are zero-based.


.. doxygenclass:: aare::RawFile
   :members:
   :undoc-members:
   :private-members:
