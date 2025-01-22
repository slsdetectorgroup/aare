import sys
sys.path.append('/home/l_msdetect/erik/aare/build')

#Our normal python imports
from pathlib import Path
import matplotlib.pyplot as plt
import numpy as np
import boost_histogram as bh
import time

from aare import File, ClusterFinder, VarClusterFinder, ClusterFile, CtbRawFile
from aare import gaus, fit_gaus

base = Path('/mnt/sls_det_storage/moench_data/Julian/MOENCH05/20250113_first_xrays_redo/raw_files/')
cluster_file = Path('/home/l_msdetect/erik/tmp/Cu.clust')

t0 = time.perf_counter()
offset= -0.5
hist3d = bh.Histogram(
    bh.axis.Regular(160, 0+offset, 160+offset),  #x
    bh.axis.Regular(150, 0+offset, 150+offset),  #y
    bh.axis.Regular(200, 0, 6000), #ADU
)

total_clusters = 0
with ClusterFile(cluster_file, chunk_size = 1000) as f:
    for i, clusters in enumerate(f):
        arr = np.array(clusters)
        total_clusters += clusters.size
        hist3d.fill(arr['y'],arr['x'], clusters.sum_2x2()) #python talks [row, col] cluster finder [x,y]

        
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

