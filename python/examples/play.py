import matplotlib.pyplot as plt
import numpy as np
plt.ion()

import aare
from aare import CtbRawFile
print('aare imported')
from aare import transform
print('transform imported')
from pathlib import Path

base = Path('~/data/aare_test_data/clusters').expanduser()


# with ClusterFile(base / 'beam_En700eV_-40deg_300V_10us_d0_f0_100.clust') as f:
#     clusters = f.read_clusters(100)


with ClusterFile(base / 'single_frame_97_clustrers.clust', chunk_size=10) as f:
    for clusters in f:
        print(clusters.size)


#target format
# [frame, counter, row, col]
# plt.imshow(data[0,0])


base = Path('/mnt/sls_det_storage/matterhorn_data/aare_test_data/ci/aare_test_data')
# p = Path(base / 'jungfrau/jungfrau_single_master_0.json')

# f = aare.File(p)
# for i in range(10):
#     frame = f.read_frame()



# # f2 = aare.CtbRawFile(fpath, transform=transform.matterhorn02)
# # header, data = f2.read()
# # plt.plot(data[:,0,20,20])
# from aare import RawMasterFile, File, RawSubFile, DetectorType, RawFile
# base = Path('/mnt/sls_det_storage/matterhorn_data/aare_test_data/Jungfrau10/Jungfrau_DoubleModule_1UDP_ROI/SideBySide/')
# fpath = Path('241019_JF_12keV_Si_FF_GaAs_FF_7p88mmFilter_PedestalStart_ZPos_5.5_master_0.json')
# raw = Path('241019_JF_12keV_Si_FF_GaAs_FF_7p88mmFilter_PedestalStart_ZPos_5.5_d0_f0_0.raw')



# m = RawMasterFile(base / fpath)
# # roi = m.roi
# # rows = roi.ymax-roi.ymin+1
# # cols = 1024-roi.xmin
# # sf = RawSubFile(base / raw, DetectorType.Jungfrau, rows, cols, 16)

from aare import RawFile



from aare import RawFile, File

base = Path('/mnt/sls_det_storage/matterhorn_data/aare_test_data/Jungfrau10/Jungfrau_DoubleModule_1UDP_ROI/')
fname = base / Path('SideBySide/241019_JF_12keV_Si_FF_GaAs_FF_7p88mmFilter_PedestalStart_ZPos_5.5_master_0.json')
# fname = Path(base / 'jungfrau/jungfrau_single_master_0.json')
# fname = base / 'Stacked/241024_JF10_m450_m367_KnifeEdge_TestBesom_9keV_750umFilter_PedestalStart_ZPos_-6_master_0.json'


f = RawFile(fname)
h,img = f.read_frame()
print(f'{h["frameNumber"]}')
