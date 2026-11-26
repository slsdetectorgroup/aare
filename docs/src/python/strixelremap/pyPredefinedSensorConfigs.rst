
.. _python_predefined_sensor_configs:

Predefined Sensor Configurations
=================================

Includes all predefined sensor configurations for Jungfrau iLGAD (inverse Low-Gain Avalache Diode) and Jungfrau TEW (Thin Entrance Window) sensors.

The PSI Photon Science Detector Group produces Jungfrau modules with three different sensor types: 2x2 cm iLGAD, 2x2 cm TEW and 4x4 cm iLGAD. 

Below are the predefined sensor configurations for all three sensor types. Detailed information about the sensor geometry, placement and rotation on the module, strixel types and their placement can be found in the dedicated sensor sections further below.


Predefined Sensor Configurations
---------------------------------

.. py:currentmodule:: aare.strixelremap

.. data:: SingleChipMP_iLGAD

    Sensor configuration of the 2x2 cm iLGAD sensor with all strixel groups. 

.. data:: SingleChipMP_TEW

    Sensor configuration of the 2x2 cm TEW sensor with all strixel groups. 

.. data:: Quad_iLGAD

    Sensor configuration of the 4x4 cm iLGAD sensor with all strixel groups. 

.. _python_predefined_strixel_groups:

Predefined Strixel Groups
---------------------------------

.. data:: StrxP25

    Strixel geometry for 25 µm pitch strixels on iLGAD sensors (multiplicity = 3) 

.. data:: StrxP15

    Strixel geometry for 15 µm pitch strixels on iLGAD sensors (multiplicity = 5)

.. data:: StrxP18

    Strixel geometry for 18.75 µm pitch strixels on iLGAD sensors (multiplicity = 4)

.. data:: StrxP37

    Strixel geometry for 37.5 µm pitch strixels on iLGAD sensors (multiplicity = 2)


.. _python_predefined_sensor_placements:

Predefined Sensor Placements on Junfrau Modules
------------------------------------------------

The PSI Photon Science Detector Group produces Jungfrau modules with a 2x2 cm iLGAD/TEW sensor placed on the position of the second chip (Chip1) and the seventh chip (Chip6) of the module. 
The larger 4x4cm iLGAD sensors are typically placed on the module's quad (Chip1+Chip2+Chip5+Chip6). 

.. data:: Chip1

    Placement of the 2x2cm iLGAD sensor on the second chip (Chip1) of the Jungfrau module, with no rotation applied. 

.. data:: Chip6

    Placement of the 2x2cm iLGAD sensor on the seventh chip (Chip6) of the Jungfrau module, with a 180-degree rotation applied. 

.. data:: Quad 

    Placement of the 4x4cm iLGAD sensor on the quad (Chip1+Chip2+Chip5+Chip6) of the Jungfrau module, with no rotation applied. 

2x2 cm iLGAD Sensor: 
---------------------

The 2x2 cm iLGAD sensor has a guardring that extends into (and renders unusable for photon detection) part of the active pixel area with height and width of 9 pixels each and spawns over 256x256 pixels. 

.. data:: SingleChipMP_iLGAD_pix

    Pixel geometry for the 2x2 cm iLGAD sensor. 

The PSI Photon Science Detector Group produces Jungfrau modules with 2x2 cm iLGAD sensors placed on the position of the second chip (Chip1) ``Chip1`` and the seventh chip (Chip6) ``Chip6`` of the module.
See :ref:`python_predefined_sensor_placements` for more information.

The sensor is partitioned into three strixel groups ``StrxP25``, ``StrxP15`` and ``StrxP18``. See :ref:`python_predefined_strixel_groups` for more information.

Strixel group ``StrxP25`` covers the bottom 25 % of the sensor's pixel area (minus the guard ring pixels). Strixel Group ``StrxP15`` covers the next 25 % of the sensor's pixel area. Strixel group ``StrxP18`` covers the top 50 % of the sensor's pixel, whereby it is divided into two partitions (each 25 % of the pixel area) with different dimensioning of the metal layers on top of strixel implants.

.. data:: SingleChipMP_iLGAD_P25 

    Strixel group of 25 µm pitch strixels on the 2x2 cm iLGAD sensor.

.. data:: SingleChipMP_iLGAD_P15

    Strixel group of 15 µm pitch strixels on the 2x2 cm iLGAD sensor.

.. data:: SingleChipMP_iLGAD_P18

    Strixel group of 18.75 µm pitch strixels on the 2x2 cm iLGAD sensor.

2x2 cm TEW Sensor: 
---------------------

The 2x2 cm TEW sensor has no guardring extending into the active pixel area and spawns over 256x256 pixels. 

.. data:: SingleChipMP_TEW_pix

    Pixel geometry for the 2x2 cm TEW sensor.

The Photon Detector Group produces Jungfrau modules with 2x2 cm TEW sensors placed on the position of the second chip (Chip1) ``Chip1`` and the seventh chip (Chip6) ``Chip6`` of the module.
See :ref:`python_predefined_sensor_placements` for more information.

The sensor is again partitioned into three pixel groups ``StrxP25``, ``StrxP15`` and ``StrxP18``. See :ref:`python_predefined_strixel_groups` for more information.

The partition is the same as for the 2x2 cm iLGAD sensor.

.. data:: SingleChipMP_TEW_P25

    Strixel group of 25 µm pitch strixels on the 2x2 cm TEW sensor.

.. data:: SingleChipMP_TEW_P15

    Strixel group of 15 µm pitch strixels on the 2x2 cm TEW sensor.

.. data:: SingleChipMP_TEW_P18

    Strixel group of 18.75 µm pitch strixels on the 2x2 cm TEW sensor.
    
4x4 cm iLGAD Sensor:
---------------------

The 4x4 cm iLGAD sensor has a guardring height and width of 9 pixels each and spawns over 512x512 pixels.

.. data:: Quad_iLGAD_pix

    Pixel geometry for the 4x4 cm iLGAD sensor.

The 4x4 cm iLGAD sensor is typically placed on the module's quad (Chip1+Chip2+Chip5+Chip6) ``Quad``. See :ref:`python_predefined_sensor_placements` for more information.

The sensor is partitioned into two strixel groups ``StrxP25``. See :ref:`python_predefined_strixel_groups` for more information. The bottom sensor half uses forward pixel-to-strixel routing and the top sensor half uses backward pixel to strixel routing.

.. data:: Quad_iLGAD_bottomhalf

    Strixel group of 25 µm pitch strixels located on the bottom half of 4x4 cm iLGAD sensor. 

.. data:: Quad_iLGAD_tophalf

    Strixel group of 25 µm pitch strixels located on the top half of 4x4 cm iLGAD sensor. 



