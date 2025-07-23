
Calibration
==============

Functions for applying calibration to data.

.. code-block:: python

    import aare

    # Load calibration data for a single JF module (512x1024 pixels)
    calibration = aare.load_calibration('path/to/calibration/file.bin')

    raw_data = ...  # Load your raw data here
    pedestal = ...  # Load your pedestal data here

    # Apply calibration to raw data to convert from raw ADC values to keV
    data = aare.apply_calibration(raw_data, pd=pedestal, cal=calibration)

    # If you pass a 2D pedestal and calibration only G0 will be used for the conversion
    # Pixels that switched to G1 or G2 will be set to 0
    data = aare.apply_calibration(raw_data, pd=pedestal[0], cal=calibration[0])

    

.. py:currentmodule:: aare

.. autofunction:: apply_calibration

.. autofunction:: load_calibration

.. autofunction:: calculate_pedestal

.. autofunction:: calculate_pedestal_float

.. autofunction:: calculate_pedestal_g0

.. autofunction:: calculate_pedestal_g0_float

.. autofunction:: count_switching_pixels
