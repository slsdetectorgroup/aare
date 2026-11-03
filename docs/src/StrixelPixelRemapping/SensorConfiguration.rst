.. _sensor_configuration:

Sensor Configuration
=====================


Helper structs for strong-typing to define the sensor configuration, such as sensor pixel geometry, placement and rotation of the sensor
on the module, the different strixel types on the sensor and its placement.

An introduction to the concept of strixel-to-pixel remapping and and overview of the corresponding API can be found
in :ref:`strixel_remapping_index`.

Inputs for the Remapping Algorithm
------------------------------------

The most central input for the :ref:`remap_algorithm` is the sensor configuration given by :code:`SensorConfig`. The :ref:`map_generators` use
:ref:`predefined_sensor_configs` to directly generate the corresponding strixel-to-pixel maps.

.. doxygenstruct:: aare::remap::defs::SensorConfig
    :members:

The detailed documentation of all components of :code:`SensorConfig` can be found below in :ref:`sensor_config_components`.

To correctly anchor the coordinate system of the remapping, one can further pass the placement of the sensor on the module,
including its orientation, and specify any potential relative shift of the bump bonding alignment between sensor and ASIC.

.. doxygenstruct:: aare::remap::defs::SensorModulePlacement
    :members:

.. doxygenenum:: aare::remap::defs::Rotation

.. doxygenstruct:: aare::remap::defs::BondShift
    :members:

.. Note::
    The :code:`BondShift` is defined within the local sensor coordinate system before any rotation has been applied, i.e.
    sensor and ASIC assembly are rotated as a unity.

.. _sensor_config_components:   

Components of SensorConfig
^^^^^^^^^^^^^^^^^^^^^^^^^^^^

.. doxygenstruct:: aare::remap::defs::SensorPixelGeometry
    :members:

.. doxygenstruct:: aare::remap::defs::Guardring
    :members:

.. NOTE::
    While (multiple) guardrings are part of every sensor, iLGAD sensors
    (due to their high currents and electric fields) have an additional guardring
    that extends into the area of the sensor that would normally be
    occupied by active pixels. This area and the corresponding pixels therefore
    become unusable for photon detection.
    For standard (non-iLGAD) sensors, the regular guardrings are outside the
    pixel area and do not need to be specified with this struct.

.. IMPORTANT::
    The :code:`Guardring` struct describes only the guardring that extends into the sensor pixel area.

.. doxygenstruct:: aare::remap::defs::GroupConfig
    :members:

Components of GroupConfig
"""""""""""""""""""""""""""

.. doxygenstruct:: aare::remap::defs::GroupStrixelGeometry
    :members:

.. doxygenstruct:: aare::remap::defs::GroupRouting
    :members:

.. doxygenenum:: aare::remap::defs::ModuloOrdering


Output of Remapping Algorithm
-------------------------------

.. doxygenstruct:: aare::remap::defs::StrixelGroupToPixelMap
    :members:

.. IMPORTANT::
    The map coordinates (row, col) are local to this strixel group and
    are geometrically associated with :code:`effective_roi`. The stored pixel index,
    however, is flattened with respect to the original user-provided ROI,
    not :code:`effective_roi`. In other words: The map provides a local strixel grid mapped to
    the corresponding ASIC pixel indices in the original user grid, and
    :code:`effective_roi` is the ROI the algorithm used for remapping.