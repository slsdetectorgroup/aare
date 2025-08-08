#Calibration related functions
import numpy as np
def load_calibration(fname, hg0=False):
    """
    Load calibration data from a file.
    
    Parameters:
    fname (str): Path to the calibration file.
    hg0 (bool): If True, load HG0 calibration data instead of G0.

    """
    gains = 3
    rows = 512
    cols = 1024
    with open(fname, 'rb') as f:
        cal = np.fromfile(f, count=gains * rows * cols, dtype=np.double).reshape(
            gains, rows, cols
        )
        if hg0:
            cal[0] = np.fromfile(f, count=rows * cols, dtype=np.double).reshape(rows, cols)
    return cal