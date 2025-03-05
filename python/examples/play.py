import sys
sys.path.append('/home/l_msdetect/erik/aare/build')


#Our normal python imports
from pathlib import Path
import matplotlib.pyplot as plt
from mpl_toolkits.axes_grid1 import make_axes_locatable
import numpy as np
import boost_histogram as bh
import time

import tifffile

#Directly import what we need from aare
from aare import File, ClusterFile, hitmap
from aare._aare import calculate_eta2, ClusterFinderMT, ClusterCollector


base = Path('/mnt/sls_det_storage/moench_data/tomcat_nanoscope_21042020/09_Moench_650um/')

# for f in base.glob('*'):
#     print(f.name)

cluster_fname = base/'acq_interp_center_3.8Mfr_200V.clust'
flatfield_fname = base/'flatfield_center_200_d0_f000000000000_0.clust'

cluster_fname.stat().st_size/1e6/4

image = np.zeros((400,400))
with ClusterFile(cluster_fname, chunk_size = 1000000) as f:
    for clusters in f:
        test = hitmap(image.shape, clusters)
        break
        # image += hitmap(image.shape, clusters)
        # break
print('We are back in python')
# fig, ax = plt.subplots(figsize = (7,7))
# im = ax.imshow(image)
# im.set_clim(0,1)