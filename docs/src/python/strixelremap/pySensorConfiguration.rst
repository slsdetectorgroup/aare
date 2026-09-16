Sensor configurations
=====================

Helper structs to define the sensor configuration, such as sensor pixel geometry, placement and rotation of the sensor on the module, the different strixel types on the sensor and its placement. 

.. py:currentmodule:: aare.strixelremap

.. autoclass:: Rotation
    :members:

.. autoclass:: ModuloOrdering
    :members: 
    
.. autoclass:: BondShift
    :members:
    :special-members: __init__

.. autoclass:: Guardring
    :members:
    :special-members: __init__

.. autoclass:: GroupRouting
    :members:
    :special-members: __init__

.. autoclass:: SensorPixelGeometry
    :members:
    :special-members: __init__


.. autoclass:: GroupStrixelGeometry
    :members:
    :special-members: __init__

.. autoclass:: GroupConfig
    :members:
    :special-members: __init__

.. autoclass:: SensorModulePlacement
    :members:
    :special-members: __init__

.. autoclass:: StrixelGroupToPixelMap
    :members:
    :special-members: __init__

**Helper functions to create SensorConfig with N pixel groups:**

.. autofunction:: SensorConfig
   :noindex:


.. code:: python

    from aare import strixelremap 

    SensorConfig(strixelremap.SingleChipMP_iLGAD_pix, [strixelremap.StrxP15, strixelremap.StrxP25]) # returns a SensorConfig_2PixelGroups 