Pixel to Strixel Remapping 
============================


.. toctree::
   :caption: Pixel to Strixel Remapping
   :maxdepth: 1

   pySensorConfiguration
   pyStrixelPixelRemapAlgorithm
   pyInclusiveROI
   pyPredefinedMaps
   pyPredefinedSensorConfigs


Example Usage
-------------

.. code:: python

   from aare import strixelremap 
   import numpy as np

   from aare import RawFile 

   file = RawFile("path/to/master_file.json") # Load a raw file with strixel data

   _, frame = file.read_frame() # Read the first frame of the file

   rx_roi = frame.master().rois[0] # Get ROI of the frame

   # Get the pixel to strixel map for the 25 µm pitch strixels placed on Chip1
   pixel_to_strixel_map_25um = strixelremap.jungfrau_ilgad_singlechip_25um_strixel_map(user_roi = strixelremap.toInclusiveROI(rx_roi), placement = strixelremap.Chip1)


   # map(row, col) gives flattened index of the pixel mapped to strixel at (row, col) 
   order_map = pixel_to_strixel_map_25um.map

   strixels = np.empty(order_map.shape, dtype=np.uint16)

   # Apply the remapping to a given strixel data array
   strixelremap.apply_remap(frame.astype(np.uint16), order_map, strixels)
  