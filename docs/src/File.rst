File
=============

``File`` opens ``.json`` and ``.raw`` master files, ``.npy`` files and
Jungfrau ``.dat`` files behind one interface. ``FileConfig::gap_pixels``
inserts :doc:`gap pixels <GapPixels>` when reading raw files; other file
types reject a set value with ``std::invalid_argument``.


.. doxygenclass:: aare::File
   :members:
   :undoc-members:
   :private-members: