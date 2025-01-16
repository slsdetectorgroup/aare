import sys
sys.path.append('/home/l_msdetect/erik/aare/build')

#Our normal python imports
from pathlib import Path
import matplotlib.pyplot as plt
import numpy as np
import boost_histogram as bh
import time

from aare import File, ClusterFinder, VarClusterFinder, ClusterFile, CtbRawFile

base = Path('/mnt/sls_det_storage/moench_data/Julian/MOENCH05/20250113_xrays_scan_energy/raw_files/')
cluster_file = Path('/home/l_msdetect/erik/tmp/Cu.clust')
pedestal_file = base/'moench05_noise_Cu_10_us_master_0.json'
data_file = base/'moench05_xray_Cu_10_us_master_0.json'

from aare._aare import fit_gaus
from aare.transform import moench05

offset = -0.5
hist3d = bh.Histogram(
    bh.axis.Regular(160, 0+offset, 160+offset),  #x
    bh.axis.Regular(150, 0+offset, 150+offset),  #y
    bh.axis.Regular(100, 3000, 4000), #ADU
)


rows = np.zeros(160*150)
cols = np.zeros(160*150)

for row in range(160):
    for col in range(150):
        rows[row*150+col] = row
        cols[row*150+col] = col

#TODO how can we speed up the filling of the 3d histogram?
with CtbRawFile(pedestal_file, transform=moench05) as f:
    for i in range(1000):
        print(f'{i}', end='\r')
        frame_number, frame = f.read_frame()
        hist3d.fill(rows, cols, frame.flat)

data = hist3d.values()
x = hist3d.axes[2].centers
a = fit_gaus(data, x)




# from aare._aare import ClusterFinderMT, ClusterCollector, ClusterFileSink


# cf = ClusterFinderMT((400,400), (3,3), n_threads = 3)
# # collector = ClusterCollector(cf)
# out_file = ClusterFileSink(cf, "test.clust")

# for i in range(1000):
#     img = f.read_frame()
#     cf.push_pedestal_frame(img)
# print('Pedestal done')
# cf.sync()

# for i in range(100):
#     img = f.read_frame()
#     cf.find_clusters(img)


# # time.sleep(1)
# cf.stop()  
# time.sleep(1)
# print('Second run')
# cf.start()
# for i in range(100):
#     img = f.read_frame()
#     cf.find_clusters(img)

# cf.stop()
# print('Third run')
# cf.start()
# for i in range(129):
#     img = f.read_frame()
#     cf.find_clusters(img)

# cf.stop()
# out_file.stop()
# print('Done')


# cfile = ClusterFile("test.clust")
# i = 0
# while True:
#     try:
#         cv = cfile.read_frame()
#         i+=1
#     except RuntimeError:
#         break
# print(f'Read {i} frames') 




# # cf = ClusterFinder((400,400), (3,3))
# # for i in range(1000):
# #     cf.push_pedestal_frame(f.read_frame())

# # fig, ax = plt.subplots()
# # im = ax.imshow(cf.pedestal())
# # cf.pedestal()
# # cf.noise()



# # N = 500
# # t0 = time.perf_counter()
# # hist1 = bh.Histogram(bh.axis.Regular(40, -2, 4000))
# # f.seek(0)

# # t0 = time.perf_counter()
# # data = f.read_n(N)
# # t_elapsed = time.perf_counter()-t0


# # n_bytes = data.itemsize*data.size

# # print(f'Reading {N} frames took {t_elapsed:.3f}s {N/t_elapsed:.0f} FPS, {n_bytes/1024**2:.4f} GB/s')


# # for frame in data:
# #     a = cf.find_clusters(frame)

# # clusters = cf.steal_clusters()

# # t_elapsed = time.perf_counter()-t0
# # print(f'Clustering {N} frames took {t_elapsed:.2f}s  {N/t_elapsed:.0f} FPS')


# # t0 = time.perf_counter()
# # total_clusters = clusters.size

# # hist1.fill(clusters.sum())

# # t_elapsed = time.perf_counter()-t0
# # print(f'Filling histogram with the sum of {total_clusters} clusters took: {t_elapsed:.3f}s, {total_clusters/t_elapsed:.3g} clust/s')
# # print(f'Average number of clusters per frame {total_clusters/N:.3f}')