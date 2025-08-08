# Make the compiled classes that live in _aare available from aare.
from . import _aare


from ._aare import File, RawMasterFile, RawSubFile, JungfrauDataFile
from ._aare import Pedestal_d, Pedestal_f, ClusterFinder_Cluster3x3i, VarClusterFinder
from ._aare import DetectorType
from ._aare import hitmap
from ._aare import ROI

# from ._aare import ClusterFinderMT, ClusterCollector, ClusterFileSink, ClusterVector_i

from .ClusterFinder import ClusterFinder, ClusterCollector, ClusterFinderMT, ClusterFileSink, ClusterFile
from .ClusterVector import ClusterVector


from ._aare import fit_gaus, fit_pol1, fit_scurve, fit_scurve2
from ._aare import Interpolator
from ._aare import calculate_eta2


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
