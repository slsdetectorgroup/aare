# SPDX-License-Identifier: MPL-2.0


from . import _aare 
import numpy as np
from .ClusterFinder import _get_class

def ClusterVector(cluster_size=(3,3), dtype = np.int32):
    """
    Create an empty ClusterVector for a supported cluster size and pixel dtype.

    Parameters
    ----------
    cluster_size : tuple[int, int], default=(3, 3)
        Cluster dimensions in the x and y directions.
    dtype : numpy.dtype, default=numpy.int32
        Pixel storage type. The cluster size and dtype combination must have a
        compiled binding.

    Returns
    -------
    ClusterVector
        An empty vector with frame number 0 and space reserved for at least
        1024 clusters.

    Raises
    ------
    ValueError
        If the requested size and dtype combination is unavailable.

    Examples
    --------
    >>> import numpy as np
    >>> from aare import ClusterVector
    >>> clusters = ClusterVector(cluster_size=(3, 3), dtype=np.float64)
    """

    cls = _get_class("ClusterVector", cluster_size, dtype)
    return cls()
