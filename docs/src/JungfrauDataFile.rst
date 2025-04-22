JungfrauDataFile
==================

JungfrauDataFile is a class to read the .dat files that are produced by Aldo's receiver. 
It is mostly used for calibration. 

The structure of the file is:

* JungfrauDataHeader
* Binary data (256x256, 256x1024 or 512x1024)
* JungfrauDataHeader
* ...

There is no metadata indicating number of frames or the size of the image, but this
will be infered by this reader. 

.. doxygenstruct:: aare::JungfrauDataHeader
   :members:
   :undoc-members:
   :private-members:

.. doxygenclass:: aare::JungfrauDataFile
   :members:
   :undoc-members:
   :private-members: