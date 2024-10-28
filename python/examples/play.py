import matplotlib.pyplot as plt
import numpy as np
plt.ion()

import aare
from pathlib import Path

p = Path('/Users/erik/data/aare_test_data/jungfrau/jungfrau_single_master_0.json')

f = aare.File(p)
frame = f.read_frame()

fig, ax = plt.subplots()
im = ax.imshow(frame, cmap='viridis')