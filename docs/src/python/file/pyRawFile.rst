RawFile
===================

.. py:currentmodule:: aare

``total_frames`` and ``len(reader)`` report the minimum actual frame count
across all selected subfiles and ROIs, including each subfile's complete
``.raw`` series. ``read()`` uses this count, and ``read_n()`` and
``read_n_with_roi()`` limit batches to the remaining frames. Counts are
determined when the reader is constructed.
An empty subfile makes the count zero, and construction fails if no subfiles
are selected.

The C++ logger prints one warning to stderr on opening if the subfile counts
differ or their minimum differs from ``reader.master.frames_in_file``. The
warning includes the count range, the recorded master count, and the count
used for reading. Master metadata is preserved; its expected frame count
does not affect reading bounds. Detector frame-number mismatches can still
cause synchronization errors within these bounds.

Reading errors raise ``RuntimeError`` with the attempted zero-based frame index
and the failing file path. If a data subfile fails, its path and the underlying
failure reason are reported. Out-of-range subfile errors include the available frame count across
that subfile's series. This applies to ``read_frame()``, ``read_n()``,
and the ROI reading methods. The frame index is the position in the file, not
the frame number recorded by the detector.

Synchronization errors also identify the last ``.raw`` data file read for the
module that ran out of frames. An out-of-range request rejected before accessing
a subfile reports the master path.

For a file containing five frames, the valid indices are 0 through 4. Attempting
to read index 5 raises an error stating: ``Frame index 5 is out of range: file
contains 5 frames (indices are zero-based)``.

.. autoclass:: RawFile
    :members:
    :undoc-members:
    :show-inheritance:
    :inherited-members:
