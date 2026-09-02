.. _predefined_sensor_configs:

Predefined Sensor Configurations
=================================

Includes all predefined sensor configurations for Jungfrau iLGAD (inverse Low-Gain Avalache Diode) and Jungfrau TEW (Thin Entrance Window) sensors.

The PSI Photon Science Detector Group produces Jungfrau modules with three different sensor types: 2x2 cm iLGAD, 2x2 cm TEW and 4x4 cm iLGAD. 

Below are the predefined sensor configurations for all three sensor types. Detailed information about the sensor geometry, placement and rotation on the module, strixel types and their placement can be found in the dedicated sensor sections further below.


Predefined Sensor Configurations
---------------------------------

.. doxygenvariable:: aare::remap::config::jungfrau::SingleChipMP_iLGAD

.. doxygenvariable:: aare::remap::config::jungfrau::SingleChipMP_TEW

.. doxygenvariable:: aare::remap::config::jungfrau::Quad_iLGAD

.. _predefined_strixel_groups:

Predefined Strixel Groups
---------------------------------

.. doxygenvariable:: aare::remap::config::jungfrau::StrxP25

.. doxygenvariable:: aare::remap::config::jungfrau::StrxP15

.. doxygenvariable:: aare::remap::config::jungfrau::StrxP18

.. doxygenvariable:: aare::remap::config::jungfrau::StrxP37


.. _predefined_sensor_placements:

Predefined Sensor Placements on Junfrau Modules
------------------------------------------------

The PSI Photon Science Detector Group produces Jungfrau modules with a 2x2 cm iLGAD/TEW sensor placed on the position of the second chip (Chip1) and the seventh chip (Chip6) of the module. 
The larger 4x4cm iLGAD sensors are typically placed on the module's quad (Chip1+Chip2+Chip5+Chip6). 

.. doxygenvariable:: aare::remap::config::jungfrau::Chip1

.. doxygenvariable:: aare::remap::config::jungfrau::Chip6

.. doxygenvariable:: aare::remap::config::jungfrau::Quad 

2x2 cm iLGAD Sensor: 
---------------------

The 2x2 cm iLGAD sensor has a guardring that extends into (and renders unusable for photon detection) part of the active pixel area with height and width of 9 pixels each and spawns over 256x256 pixels. 

.. doxygenvariable:: aare::remap::config::jungfrau::SingleChipMP_iLGAD_pix

The PSI Photon Science Detector Group produces Jungfrau modules with 2x2 cm iLGAD sensors placed on the position of the second chip (Chip1) ``Chip1`` and the seventh chip (Chip6) ``Chip6`` of the module.
See :ref:`predefined_sensor_placements` for more information.

The sensor is partitioned into three strixel groups ``StrxP25``, ``StrxP15`` and ``StrxP18``. See :ref:`predefined_strixel_groups` for more information.

Strixel group ``StrxP25`` covers the bottom 25 % of the sensor's pixel area (minus the guard ring pixels). Strixel Group ``StrxP15`` covers the next 25 % of the sensor's pixel area. Strixel group ``StrxP18`` covers the top 50 % of the sensor's pixel, whereby it is divided into two partitions (each 25 % of the pixel area) with different dimensioning of the metal layers on top of strixel implants.

.. doxygenvariable:: aare::remap::config::jungfrau::SingleChipMP_iLGAD_P25 

.. doxygenvariable:: aare::remap::config::jungfrau::SingleChipMP_iLGAD_P15

.. doxygenvariable:: aare::remap::config::jungfrau::SingleChipMP_iLGAD_P18

2x2 cm TEW Sensor: 
---------------------

The 2x2 cm TEW sensor has no guardring extending into the active pixel area and spawns over 256x256 pixels. 

.. doxygenvariable:: aare::remap::config::jungfrau::SingleChipMP_TEW_pix

The Photon Detector Group produces Jungfrau modules with 2x2 cm TEW sensors placed on the position of the second chip (Chip1) ``Chip1`` and the seventh chip (Chip6) ``Chip6`` of the module.
See :ref:`predefined_sensor_placements` for more information.

The sensor is again partitioned into three pixel groups ``StrxP25``, ``StrxP15`` and ``StrxP18``. See :ref:`predefined_strixel_groups` for more information.

The partition is the same as for the 2x2 cm iLGAD sensor.

.. doxygenvariable:: aare::remap::config::jungfrau::SingleChipMP_TEW_P25

.. doxygenvariable:: aare::remap::config::jungfrau::SingleChipMP_TEW_P15

.. doxygenvariable:: aare::remap::config::jungfrau::SingleChipMP_TEW_P18

4x4 cm iLGAD Sensor:
---------------------

The 4x4 cm iLGAD sensor has a guardring height and width of 9 pixels each and spawns over 512x512 pixels.

.. doxygenvariable:: aare::remap::config::jungfrau::Quad_iLGAD_pix

The 4x4 cm iLGAD sensor is typically placed on the module's quad (Chip1+Chip2+Chip5+Chip6) ``Quad``. See :ref:`predefined_sensor_placements` for more information.

The sensor is partitioned into two strixel groups ``StrxP25``. See :ref:`predefined_strixel_groups` for more information. The bottom sensor half uses forward pixel-to-strixel routing and the top sensor half uses backward pixel to strixel routing.

.. doxygenvariable:: aare::remap::config::jungfrau::Quad_iLGAD_bottomhalf

.. doxygenvariable:: aare::remap::config::jungfrau::Quad_iLGAD_tophalf




