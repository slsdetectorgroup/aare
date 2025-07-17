import pytest 
import numpy as np
from aare import apply_calibration

def test_apply_calibration_small_data():
    # The raw data consists of 10 4x5 images
    raw = np.zeros((10, 4, 5), dtype=np.uint16)

    # We need a pedestal for each gain, so 3 
    pedestal = np.zeros((3, 4, 5), dtype=np.float32)

    # And the same for calibration
    calibration = np.ones((3, 4, 5), dtype=np.float32)

    # Set the known values, probing one pixel in each gain
    raw[0, 0, 0] = 100 #ADC value of 100, gain 0
    pedestal[0, 0, 0] = 10
    calibration[0, 0, 0] = 43.7

    raw[2, 3, 3] = (1<<14) + 1000 #ADC value of 1000, gain 1
    pedestal[1, 3, 3] = 500
    calibration[1, 3, 3] = 2.0

    raw[1,1,4] = (3<<14) + 857 #ADC value of 857, gain 2
    pedestal[2,1,4] = 100
    calibration[2,1,4] = 3.0

    

    data = apply_calibration(raw, pd = pedestal, cal = calibration)


    # The formula that is applied is:
    # calibrated = (raw - pedestal) / calibration
    assert data.shape == (10, 4, 5)
    assert data[0, 0, 0] == (100 - 10) / 43.7
    assert data[2, 3, 3] ==  (1000 - 500) / 2.0
    assert data[1, 1, 4] == (857 - 100) / 3.0

    # Other pixels should be zero
    assert data[2,2,2] == 0
    assert data[0,1,1] == 0
    assert data[1,3,0] == 0
