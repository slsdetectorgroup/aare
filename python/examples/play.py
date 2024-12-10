import sys
sys.path.append('/home/l_msdetect/erik/aare/build')

#Our normal python imports
from pathlib import Path
import matplotlib.pyplot as plt
import numpy as np
import boost_histogram as bh
import time

from aare import File, ClusterFinder, VarClusterFinder

base = Path('/mnt/sls_det_storage/matterhorn_data/aare_test_data/')

f = File(base/'Moench03new/cu_half_speed_master_4.json')
cf = ClusterFinder((400,400), (3,3))
for i in range(1000):
    cf.push_pedestal_frame(f.read_frame())

fig, ax = plt.subplots()
im = ax.imshow(cf.pedestal())
cf.pedestal()
cf.noise()

N = 200
t0 = time.perf_counter()
hist1 = bh.Histogram(bh.axis.Regular(40, -2, 4000))
f.seek(0)

t0 = time.perf_counter()
data = f.read_n(N)
t_elapsed = time.perf_counter()-t0

print(f'Reading {N} frames took {t_elapsed:.3f}s {N/t_elapsed:.0f} FPS')

clusters = []
for frame in data:
    clusters += [cf.find_clusters_without_threshold(frame)]


t_elapsed = time.perf_counter()-t0
print(f'Clustering {N} frames took {t_elapsed:.2f}s  {N/t_elapsed:.0f} FPS')


t0 = time.perf_counter()
total_clusters = 0
for cl in clusters:
        arr = np.array(cl, copy = False)
        hist1.fill(arr['data'].sum(axis = 1).sum(axis = 1))
        total_clusters += cl.size
# t_elapsed = time.perf_counter()-t0
print(f'Filling histogram with {total_clusters} clusters took: {t_elapsed:.3f}s')
print(f'Cluster per frame {total_clusters/N:.3f}')