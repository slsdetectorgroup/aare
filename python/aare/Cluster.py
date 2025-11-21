from . import _aare 
import numpy as np
from .ClusterFinder import _type_to_char


def Cluster(x : int, y : int, data, cluster_size=(3,3), dtype = np.int32):
    """
    Factory function to create a Cluster object. Provides a cleaner syntax for
    the templated Cluster in C++.

    .. code-block:: python

        from aare import Cluster
        
        Cluster(cluster_size=(3,3), dtype=np.float64)
    """

    try:
        class_name = f"Cluster{cluster_size[0]}x{cluster_size[1]}{_type_to_char(dtype)}"
        cls = getattr(_aare, class_name)
    except AttributeError:
        raise ValueError(f"Unsupported combination of type and cluster size: {dtype}/{cluster_size} when requesting {class_name}")
   
    return cls(x, y, data) 