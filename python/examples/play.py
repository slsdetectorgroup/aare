import sys
sys.path.append('/home/l_msdetect/erik/aare/build')

#Our normal python imports
from pathlib import Path
import matplotlib.pyplot as plt
import numpy as np
import boost_histogram as bh
import time

from aare import File, ClusterFinder, VarClusterFinder, ClusterFile

base = Path('/mnt/sls_det_storage/matterhorn_data/aare_test_data/ci/aare_test_data/clusters/')

f = ClusterFile(base/'beam_En700eV_-40deg_300V_10us_d0_f0_100.clust')

c = f.read_clusters(100)

# for i, frame in enumerate(f):
#     print(f'{i}', end='\r')
# print()


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