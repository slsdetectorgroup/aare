# SPDX-License-Identifier: MPL-2.0
# Make the compiled classes that live in _aare available from aare.
from . import _aare

# ---- CUDA module (optional) ------------------------------------------------
# When the package was built with AARE_CUDA=ON, a sibling extension
# _aare_cuda contains the ClusterFinderCUDA_* classes. We re-export them
# onto _aare so user code can do `from aare import ClusterFinderCUDA_*`
# regardless of which .so physically hosts the class. On a CPU-only build
# the import fails silently and ClusterFinderCUDA_* classes simply aren't
# present; the factory in ClusterFinder.py handles that case with a clear
# RuntimeError.
try:
    from . import _aare_cuda as _aare_cuda_mod
    for _name in dir(_aare_cuda_mod):
        if _name.startswith("ClusterFinderCUDA"):
            setattr(_aare, _name, getattr(_aare_cuda_mod, _name))
    del _name
except ImportError:
    pass

from . import transform

from ._aare import File, RawMasterFile, RawSubFile, JungfrauDataFile
from ._aare import Pedestal_d, Pedestal_f, ClusterFinder_Cluster3x3i, VarClusterFinder
from ._aare import DetectorType, ReadoutMode 
from ._aare import hitmap
from ._aare import ROI
from ._aare import corner 

# from ._aare import ClusterFinderMT, ClusterCollector, ClusterFileSink, ClusterVector_i

from .ClusterFinder import ClusterFinder, ClusterCollector, ClusterFinderMT, ClusterFileSink, ClusterFile
from .ClusterFinder import ClusterFinderCUDA, _cuda_available
from .ClusterVector import ClusterVector
from .Cluster import Cluster

from ._aare import Gaussian, RisingScurve, FallingScurve, Pol1, Pol2
from ._aare import fit
from ._aare import fit_gaus, fit_pol1, fit_scurve, fit_scurve2
from ._aare import Interpolator
from ._aare import calculate_eta2, calculate_eta3, calculate_cross_eta3, calculate_full_eta2
from ._aare import reduce_to_2x2, reduce_to_3x3

from ._aare import apply_custom_weights

from .CtbRawFile import CtbRawFile
from .RawFile import RawFile
from .ScanParameters import ScanParameters

from .utils import random_pixels, random_pixel, flat_list, add_colorbar


#make functions available in the top level API
from .func import *

from .calibration import *
from ._aare import apply_calibration, count_switching_pixels
from ._aare import calculate_pedestal, calculate_pedestal_float, calculate_pedestal_g0, calculate_pedestal_g0_float

from ._aare import VarClusterFinder
