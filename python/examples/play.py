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


        
t_elapsed = time.perf_counter()-t0
print(f'Histogram filling took: {t_elapsed:.3f}s {total_clusters/t_elapsed/1e6:.3f}M clusters/s')

histogram_data = hist3d.counts()
x = hist3d.axes[2].edges[:-1]

y = histogram_data[100,100,:]
xx = np.linspace(x[0], x[-1])
# fig, ax = plt.subplots()
# ax.step(x, y, where = 'post')

y_err = np.sqrt(y)
y_err = np.zeros(y.size)
y_err += 1

# par = fit_gaus2(y,x, y_err)
# ax.plot(xx, gaus(xx,par))
# print(par)

res = fit_gaus(y,x)
res2 = fit_gaus(y,x, y_err)
print(res)
print(res2)

