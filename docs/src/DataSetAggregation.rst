DataSet Aggregation
===================

Operations to read file and directly perform aggregation over the dataset, e.g. mean over all frames per sample.

Both functions open the file themselves, read it in parallel and return a single 3D array.

The result has shape ``(rows, cols, num_samples)``, i.e. the sample index is the **last** axis.
``result(r, c, i)`` is the value of pixel ``(r, c)`` for sample ``i``.

Mean over a dataset
-------------------

:cpp:func:`aare::mean_over_dataset` averages a fixed number of consecutive frames for each sample.
The frames in the file are expected to be grouped by sample:

.. code-block:: text

    | sample 0                     | sample 1                     | ... |
    | frame 0 ... frame (n - 1)    | frame 0 ... frame (n - 1)    | ... |

    n = num_frames_per_sample

* The file must contain exactly ``num_samples * num_frames_per_sample`` frames, otherwise a
  ``std::runtime_error`` is thrown.
* The template parameter ``FRAME_TYPE`` must match the pixel type stored in the file
  (e.g. ``uint16_t``); it is also the type of the returned array.
* The sum is accumulated in ``double`` to avoid overflow, and the mean is cast back to
  ``FRAME_TYPE``. For integer types the fractional part is truncated.
* ``num_threads`` must be at least 1, otherwise a ``std::invalid_argument`` is thrown.

.. code-block:: cpp

    #include "aare/DataSetAggregation.hpp"

    // 10 samples (e.g. exposure times) with 1000 frames each
    auto mean = aare::mean_over_dataset<uint16_t>("path/to/master_0.json", 10, 1000);
    // mean.shape() == {rows, cols, 10}

.. doxygenfunction:: aare::mean_over_dataset

Mean of current source data
---------------------------

:cpp:func:`aare::mean_current_source` reads a Jungfrau data file recorded during a current source scan. In such a scan only every 64th
column is illuminated in each frame. Consecutive frames step through the 64 columns of a
subcolumn group, so frame ``k`` (counted from the start of the sample) illuminates column
``k % 64`` in each group of 64 columns:

.. code-block:: text

    frame:            0   1   2  ...  63   64   65  ...
    illuminated col:  0   1   2  ...  63    0    1  ...   (+ 64 * s for every group s)

For every pixel only the frames in which its column is illuminated are used. Each sample therefore
spans ``mean_over_num_frames * 64`` frames, and every pixel is averaged over
``mean_over_num_frames`` values.

* The modules in the top half of the detector (rows ``>= rows / 2``) are mounted flipped, so for
  these rows the column index within a group is mirrored, i.e. frame ``k`` illuminates column
  ``63 - (k % 64)``.
* ``file_offset`` skips the given number of frames at the start of the file. The file must contain
  exactly ``file_offset + num_samples * mean_over_num_frames * 64`` frames, otherwise a
  ``std::runtime_error`` is thrown.
* The returned array is always ``uint16_t``. The sum is accumulated in ``uint64_t`` and the mean
  is truncated to an integer. The raw pixel values are averaged; no pedestal subtraction or gain
  conversion is applied.
* Frames are read in chunks of 16 frames (~16 MiB for a 512x1024 detector).
* ``num_threads`` must be at least 1, otherwise a ``std::invalid_argument`` is thrown.

.. code-block:: cpp

    #include "aare/DataSetAggregation.hpp"

    // 5 current source settings, 100 frames per column -> 6400 frames per sample
    auto cs = aare::mean_current_source("path/to/data_000000.dat", 100, 5);
    // cs.shape() == {rows, cols, 5}

.. doxygenfunction:: aare::mean_current_source
