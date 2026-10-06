Sensor configurations
=====================

Helper structs for strong-typing to define the sensor configuration, such as sensor pixel geometry, placement and rotation of the sensor
on the module, the different strixel types on the sensor and its placement.

An introduction to the concept of strixel-to-pixel remapping and and overview of the corresponding API can be found
in :ref:`py_strixel_remapping_index`.

.. py:currentmodule:: aare.strixelremap

.. autoclass:: Rotation
    :members:

.. autoclass:: ModuloOrdering
    :members: 
    
.. autoclass:: BondShift
    :noindex:
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

    strixelremap.SensorConfig(strixelremap.SingleChipMP_iLGAD_pix, [strixelremap.StrxP15, strixelremap.StrxP25]) # returns a SensorConfig_2PixelGroups 

