# Make the compiled classes that live in _aare available from aare.
from . import _aare


from ._aare import File, RawMasterFile, RawSubFile
from ._aare import Pedestal, ClusterFinder, VarClusterFinder
from ._aare import DetectorType
from ._aare import ClusterFile

from .CtbRawFile import CtbRawFile
from .RawFile import RawFile
from .ScanParameters import ScanParameters

from .utils import random_pixels, random_pixel

try:
    import h5py
    HDF5_FOUND = True
except ImportError:
    HDF5_FOUND = False

if HDF5_FOUND:
    from ._aare import Hdf5MasterFile
    from .Hdf5File import Hdf5File
else:
    class Hdf5MasterFile:
        def __init__(self, *args, **kwargs):
            raise ImportError("h5py library not found. HDF5 Master File is not available.")
        
    class Hdf5File:
        def __init__(self, *args, **kwargs):
            raise ImportError("h5py library not found. HDF5 File is not available.")
