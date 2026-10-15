GapPixels
===================

.. py:currentmodule:: aare

Jungfrau and Eiger modules are built from 256x256 pixel chips with double
size pixels at the chip edges. Gap pixels restore the geometry by inserting
pixels at every chip boundary inside a module and, optionally, gaps between
modules.
Gap positions are determined in detector coordinates before any ROI, so a
ROI only receives the gaps that fall strictly inside it.

The chip size (256 pixels) and the module size (1024x512 pixels) are
fixed. An Eiger quad (512x512 pixels) needs no special handling, since its
only chip boundaries are at pixel 256.

================ ================ ===========================================
Field            Default          Meaning
================ ================ ===========================================
``chip_gap``     2                Pixels inserted at each chip boundary
                                  inside a module.
``module_gaps``  None             ``ModuleGaps(x, y)`` inserted at module
                                  boundaries. None means no pixels are
                                  inserted between modules.
                                  ``ModuleGaps(8, 36)`` matches the
                                  slsDetectorPackage GUI.
``fill_value``   0                Value written to gap pixels, converted to
                                  the pixel dtype. Module gaps always
                                  receive this value.
``split_counts`` False            Split the counts of the double size edge
                                  pixels between the pixel and its gap
                                  pixel. Integer remainders go to one side
                                  at random. Requires a chip gap of 2.
``seed``         None             Seed for the random remainders.
================ ================ ===========================================

A single Jungfrau module of ``(512, 1024)`` pixels becomes ``(514, 1030)``
with the defaults. Two stacked modules become ``(1028, 1030)`` with the
defaults and ``(1064, 1030)`` with ``ModuleGaps(8, 36)``.

.. code-block:: python

    from aare import GapPixels, ModuleGaps, ROI, gapped_shape, insert_gap_pixels

    # defaults: chip gaps of 2 filled with 0, (512, 1024) -> (514, 1030)
    gapped = insert_gap_pixels(image)

    # module gaps as in the slsDetectorPackage GUI, counts split at chip edges
    gaps = GapPixels(module_gaps=ModuleGaps(8, 36), split_counts=True, seed=1)
    gapped = insert_gap_pixels(image, gaps)

    # an image that covers columns 100 to 400 of the first ten rows
    roi = ROI(100, 400, 0, 10)
    gapped_shape(roi)  # (10, 302)
    part = insert_gap_pixels(image[:10, 100:400], gaps, roi=roi)

``insert_gap_pixels`` accepts 2D ``uint8``, ``uint16``, ``uint32``,
``int32``, ``float32`` and ``float64`` arrays and returns a new array of the
same dtype. Pass ``roi`` when the image covers part of the detector.
``gapped_shape`` returns the shape of the gapped image of a ROI.

Reading files
-------------

``RawFile`` and ``File`` take ``gap_pixels``: ``True`` for the defaults or
a ``GapPixels``. Frame shapes, ``rows`` and ``cols`` then include the gaps.
Only Jungfrau and Eiger raw files are supported; other detector types raise
``ValueError``.

.. code-block:: python

    from aare import File, GapPixels, ModuleGaps, RawFile

    # defaults: chip gaps of 2 filled with 0
    with RawFile("run_master_0.json", gap_pixels=True) as f:
        header, image = f.read_frame()

    # module gaps as in the slsDetectorPackage GUI, counts split at chip edges
    gaps = GapPixels(module_gaps=ModuleGaps(8, 36), split_counts=True, seed=1)
    with File("run_master_0.json", gap_pixels=gaps) as f:
        image = f.read_frame()

.. autoclass:: GapPixels
    :members:
    :undoc-members:

.. autoclass:: ModuleGaps
    :members:
    :undoc-members:

.. autofunction:: insert_gap_pixels

.. autofunction:: gapped_shape
