
File
========

.. py:currentmodule:: aare

``File`` opens raw master files, ``.npy`` files and Jungfrau ``.dat`` files
behind one interface. ``gap_pixels`` inserts
:doc:`gap pixels <pyGapPixels>` when reading raw files; pass ``True`` for
the defaults or a ``GapPixels``. Other file types raise ``ValueError`` when
it is set.

.. autoclass:: File
    :members:
    :undoc-members:
    :show-inheritance:
    :inherited-members: