# SPDX-License-Identifier: MPL-2.0


from . import _aare 
import numpy as np
from .ClusterFinder import _get_class

def ClusterVector(cluster_size=(3,3), dtype = np.int32):
    """
    Factory function to create a ClusterVector object. Provides a cleaner syntax for
    the templated ClusterVector in C++.

    .. code-block:: python

        from aare import ClusterVector
        
        ClusterVector(cluster_size=(3,3), dtype=np.float64)
    """

    cls = _get_class("ClusterVector", cluster_size, dtype)
    return cls()

