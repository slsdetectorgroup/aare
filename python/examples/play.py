
from aare import apply_calibration
import numpy as np
raw = np.zeros((5,10,10), dtype=np.uint16)
pedestal = np.zeros((3,10,10), dtype=np.float32)
calibration = np.ones((3,10,10), dtype=np.float32)
calibrated = apply_calibration(raw, pedestal, calibration,)

