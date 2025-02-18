import sys
sys.path.append('/home/l_msdetect/erik/aare/build')

#Our normal python imports
from pathlib import Path
import matplotlib.pyplot as plt
import numpy as np
import boost_histogram as bh
import time

import aare

data = np.random.normal(10, 1, 1000)

hist = bh.Histogram(bh.axis.Regular(10, 0, 20))
hist.fill(data)


x = hist.axes[0].centers
y = hist.values()
y_err = np.sqrt(y)+1
res = aare.fit_gaus(x, y, y_err, chi2 = True)