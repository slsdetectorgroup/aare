GapPixels
=============

Jungfrau and Eiger modules are built from 256x256 pixel chips. The pixels
at a chip edge are physically double size, so an image that places the
chips next to each other is distorted. Gap pixels restore the geometry by
inserting pixels at every chip boundary inside a module and, optionally,
gaps between modules.

Gap positions are determined in detector coordinates before any ROI.
A ROI only receives the gaps that fall strictly inside it, so a ROI that
starts at column 256 has no gap on its left edge.

Configuration
-------------

``GapPixels`` holds the configuration. The chip size (256 pixels) and the
module size (1024x512 pixels, 512x512 for an Eiger quad) are fixed by the
detector type. Other detector types are rejected with
``std::invalid_argument``.

================ ================ ===========================================
Field            Default          Meaning
================ ================ ===========================================
``chip_gap``     2                Pixels inserted at each chip boundary
                                  inside a module. Source column 255 expands
                                  to the right and column 256 to the left.
``module_gaps``  unset            ``ModuleGaps{x, y}`` inserted at module
                                  boundaries. Unset means no pixels are
                                  inserted between modules.
                                  ``ModuleGaps{8, 36}`` matches the
                                  slsDetectorPackage GUI.
``fill_value``   0                Value written to gap pixels, converted to
                                  the pixel type. Out of range values wrap
                                  for integer pixels. Module gaps always
                                  receive this value.
``split_counts`` false            Split the counts of the double size edge
                                  pixels between the pixel and its gap pixel
                                  instead of writing the fill value.
``seed``         unset            Seed for the random assignment of integer
                                  remainders in split mode.
================ ================ ===========================================

Image sizes with the default configuration and with ``ModuleGaps{8, 36}``:

====================================== ========== ========= ===================
Detector range                         Ungapped   Defaults  ``ModuleGaps{8,36}``
====================================== ========== ========= ===================
One module, columns                    1024       1030      1030
Two modules side by side, columns      2048       2060      2068
Two modules stacked, rows              1024       1028      1064
ROI columns 100 to 400                 300        302       302
ROI columns 256 to 512                 256        256       256
====================================== ========== ========= ===================

Split counts
------------

With ``split_counts`` every double size pixel at a chip boundary is halved
between itself and its gap pixel. Integer pixels put an odd remainder on
one side at random; floating point pixels are halved exactly. Columns are
split first and rows second, so a pixel at a chip corner ends up in four
parts with its total preserved. Module gaps are not split and keep the
fill value. Splitting only happens where a gap pixel is inserted, so an
edge pixel on a ROI border keeps its full value. Split mode requires a
chip gap of 2.

Reading files
-------------

Pass a ``GapPixels`` to :cpp:class:`aare::RawFile` or set
``FileConfig::gap_pixels`` for :cpp:class:`aare::File` and
:cpp:class:`aare::experimental::MultiThreadedFileReader`. Frame sizes,
``rows()``, ``cols()`` and ``bytes_per_frame()`` then include the gaps.
Module data is scattered into the gapped frame while it is assembled, so no
extra frame sized buffer is needed. Headers are unchanged.

.. code-block:: cpp

   #include "aare/GapPixels.hpp"
   #include "aare/RawFile.hpp"

   aare::GapPixels gaps;
   gaps.module_gaps = aare::ModuleGaps{8, 36};
   aare::RawFile file("run_master_0.json", "r", gaps);
   auto frame = file.read_frame(); // 1064 x 1030 for two stacked modules

Images in memory
----------------

``insert_gap_pixels`` applies the same transform to an image that is already
in memory, such as a calibration map or a pedestal. The ROI gives the
detector coordinates of the image.

.. code-block:: cpp

   aare::NDArray<double, 2> pedestal = ...; // 512 x 1024
   auto gapped = aare::insert_gap_pixels<double>(
       pedestal.view(), aare::ROI{0, 1024, 0, 512},
       aare::DetectorType::Jungfrau, false, aare::GapPixels{});

``gapped_shape`` returns the shape of the gapped image of a ROI.

.. doxygenstruct:: aare::ModuleGaps
   :members:
   :undoc-members:

.. doxygenstruct:: aare::GapPixels
   :members:
   :undoc-members:

.. doxygenfunction:: aare::gapped_shape

.. doxygenfunction:: aare::insert_gap_pixels
