

from ._aare import ClusterVector_Cluster3x3i
import numpy as np

def ClusterVector(cluster_size, dtype = np.int32):

    if dtype == np.int32 and cluster_size == (3,3):
        return ClusterVector_Cluster3x3i()
    else:
        raise ValueError(f"Unsupported dtype: {dtype}. Only np.int32 is supported.")
