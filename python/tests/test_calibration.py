import pytest 
import numpy as np

import aare

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

    

    data = aare.apply_calibration(raw, pd = pedestal, cal = calibration)


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


@pytest.fixture
def raw_data_3x2x2():
    raw = np.zeros((3, 2, 2), dtype=np.uint16)
    raw[0, 0, 0] = 100
    raw[1,0, 0] =  200
    raw[2, 0, 0] = 300

    raw[0, 0, 1] = (1<<14) + 100
    raw[1, 0, 1] = (1<<14) + 200
    raw[2, 0, 1] = (1<<14) + 300

    raw[0, 1, 0] = (1<<14) + 37
    raw[1, 1, 0] =  38
    raw[2, 1, 0] = (3<<14) + 39

    raw[0, 1, 1] = (3<<14) + 100
    raw[1, 1, 1] = (3<<14) + 200
    raw[2, 1, 1] = (3<<14) + 300
    return raw

def test_calculate_pedestal(raw_data_3x2x2):
    # Calculate the pedestal
    pd = aare.calculate_pedestal(raw_data_3x2x2)
    assert pd.shape == (3, 2, 2)
    assert pd.dtype == np.float64
    assert pd[0, 0, 0] == 200
    assert pd[1, 0, 0] == 0
    assert pd[2, 0, 0] == 0

    assert pd[0, 0, 1] == 0
    assert pd[1, 0, 1] == 200
    assert pd[2, 0, 1] == 0

    assert pd[0, 1, 0] == 38
    assert pd[1, 1, 0] == 37
    assert pd[2, 1, 0] == 39

    assert pd[0, 1, 1] == 0
    assert pd[1, 1, 1] == 0
    assert pd[2, 1, 1] == 200

def test_calculate_pedestal_float(raw_data_3x2x2):
    #results should be the same for float
    pd2 = aare.calculate_pedestal_float(raw_data_3x2x2)
    assert pd2.shape == (3, 2, 2)
    assert pd2.dtype == np.float32
    assert pd2[0, 0, 0] == 200
    assert pd2[1, 0, 0] == 0
    assert pd2[2, 0, 0] == 0

    assert pd2[0, 0, 1] == 0
    assert pd2[1, 0, 1] == 200
    assert pd2[2, 0, 1] == 0

    assert pd2[0, 1, 0] == 38
    assert pd2[1, 1, 0] == 37
    assert pd2[2, 1, 0] == 39

    assert pd2[0, 1, 1] == 0
    assert pd2[1, 1, 1] == 0
    assert pd2[2, 1, 1] == 200

def test_calculate_pedestal_g0(raw_data_3x2x2):
    pd = aare.calculate_pedestal_g0(raw_data_3x2x2)
    assert pd.shape == (2, 2)
    assert pd.dtype == np.float64
    assert pd[0, 0] == 200
    assert pd[1, 0] == 38
    assert pd[0, 1] == 0
    assert pd[1, 1] == 0

def test_calculate_pedestal_g0_float(raw_data_3x2x2):
    pd = aare.calculate_pedestal_g0_float(raw_data_3x2x2)
    assert pd.shape == (2, 2)
    assert pd.dtype == np.float32
    assert pd[0, 0] == 200
    assert pd[1, 0] == 38
    assert pd[0, 1] == 0
    assert pd[1, 1] == 0

def test_count_switching_pixels(raw_data_3x2x2):
    # Count the number of pixels that switched gain
    count = aare.count_switching_pixels(raw_data_3x2x2)
    assert count.shape == (2, 2)
    assert count.sum() == 8
    assert count[0, 0] == 0 
    assert count[1, 0] == 2 
    assert count[0, 1] == 3 
    assert count[1, 1] == 3