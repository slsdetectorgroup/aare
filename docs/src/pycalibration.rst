
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

.. py:currentmodule:: aare

.. autofunction:: apply_calibration

.. autofunction:: load_calibration
