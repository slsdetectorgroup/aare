GapPixels
===================

.. py:currentmodule:: aare

Jungfrau and Eiger modules are built from 256x256 pixel chips with double
size pixels at the chip edges. Gap pixels restore the geometry by inserting
pixels at every chip boundary and, optionally, wider gaps between modules.
Gap positions are determined in detector coordinates before any ROI, so a
ROI only receives the gaps that fall strictly inside it.

Only Jungfrau and Eiger are supported; other detector types raise
``ValueError``. The chip size and the module size (1024x512, or 512x512 for
an Eiger quad) are fixed by the detector type.

================ ================ ===========================================
Field            Default          Meaning
================ ================ ===========================================
``chip_gap``     2                Pixels inserted at each chip boundary
                                  inside a module.
``module_gaps``  None             ``ModuleGaps(x, y)`` inserted at module
                                  boundaries instead of the chip gap. None
                                  means module boundaries receive the chip
                                  gap. ``ModuleGaps(8, 36)`` matches the
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

A single Jungfrau module reads as ``(514, 1030)`` with the defaults. Two
stacked modules read as ``(1030, 1030)`` with the defaults and
``(1064, 1030)`` with ``ModuleGaps(8, 36)``.

.. code-block:: python

    from aare import RawFile, File, GapPixels, ModuleGaps, insert_gap_pixels, DetectorType

    # defaults: chip gaps of 2 filled with 0
    with RawFile("run_master_0.json", gap_pixels=True) as f:
        header, image = f.read_frame()

    # module gaps as in the slsDetectorPackage GUI, counts split at chip edges
    gaps = GapPixels(module_gaps=ModuleGaps(8, 36), split_counts=True, seed=1)
    with File("run_master_0.json", gap_pixels=gaps) as f:
        image = f.read_frame()

    # the same transform for an image in memory, e.g. a pedestal
    gapped_pedestal = insert_gap_pixels(pedestal, DetectorType.Jungfrau, gaps)

``insert_gap_pixels`` accepts 2D ``uint8``, ``uint16``, ``uint32``,
``int32``, ``float32`` and ``float64`` arrays and returns a new array of the
same dtype. Pass ``roi`` when the image covers part of the detector and
``quad=True`` for an Eiger quad. ``gapped_shape`` returns the shape of the
gapped image of a ROI.

.. autoclass:: GapPixels
    :members:
    :undoc-members:

.. autoclass:: ModuleGaps
    :members:
    :undoc-members:

.. autofunction:: insert_gap_pixels

.. autofunction:: gapped_shape
