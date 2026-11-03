Strixel to Pixel Remapping
==============================

.. py:currentmodule:: aare.strixelremap


**Helper functions to create StrixelPixelMap from SensorConfig with N pixel groups:**

.. autofunction:: StrixelPixelMap
   :noindex:

.. code:: python

    from aare import strixelremap 
    
    strixelpixelmap = strixelremap.StrixelPixelMap(strixelremap.SensorConfig(strixelremap.SingleChipMP_iLGAD_pix, [strixelremap.StrxP25]), strixelremap.Chip1, strixelremap.BondShift(2, 2)) # returns a StrixelPixelMap for the custom SensorConfig with Sensored placed on Chip1 and a BondShift of 2 pixels in x and y direction



**Example of the full StrixelPixelRemap class for a SensorConfig with 3 pixel groups:**

.. autoclass:: StrixelPixelMap_3Groups_3Maps
   :special-members: __init__, __call__ 
   :members:
   :inherited-members:
