import matplotlib.pyplot as plt
import numpy as np
plt.ion()

import aare
from pathlib import Path

# p = Path('/Users/erik/data/aare_test_data/jungfrau/jungfrau_single_master_0.json')

# f = aare.File(p)
# frame = f.read_frame()

# fig, ax = plt.subplots()
# im = ax.imshow(frame, cmap='viridis')


# fpath = Path('/Users/erik/data/Moench03old/test_034_irradiated_noise_g4_hg_exptime_2000us_master_0.json')
fpath = Path('/Users/erik/data/Moench05/moench05_master_0.json')
f = aare.File(fpath)
f.seek(437)
frame = f.read_frame()

m = aare.GenerateMoench05PixelMap()
img = np.take(frame, m.astype(np.int64))

