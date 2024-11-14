import matplotlib.pyplot as plt
import numpy as np
plt.ion()
from pathlib import Path
from aare import ClusterFile

base = Path('~/data/aare_test_data/clusters').expanduser()
# f = ClusterFile(base / 'beam_En700eV_-40deg_300V_10us_d0_f0_100.clust')
f = ClusterFile(base / 'single_frame_97_clustrers.clust')


