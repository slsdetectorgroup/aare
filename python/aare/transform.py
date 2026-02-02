# SPDX-License-Identifier: MPL-2.0
import numpy as np
from . import _aare


class AdcSar04Transform64to16:
    def __call__(self, data):
        return _aare.adc_sar_04_decode64to16(data)

class AdcSar05Transform64to16:
    def __call__(self, data):
        return _aare.adc_sar_05_decode64to16(data)
    
class AdcSar05060708Transform64to16:
    def __call__(self, data):
        return _aare.adc_sar_05_06_07_08decode64to16(data)

class Moench05Transform:
    #Could be moved to C++ without changing the interface
    def __init__(self):
        self.pixel_map = _aare.GenerateMoench05PixelMap()

    def __call__(self, data):
        return np.take(data.view(np.uint16), self.pixel_map)
    

class Moench03Transform: 
    def __init__(self): 
        self.pixel_map = _aare.GenerateMoench03PixelMap()

    def __call__(self, data):
        return np.take(data.view(np.uint16), self.pixel_map)
    

class Moench05Transform1g:
    #Could be moved to C++ without changing the interface
    def __init__(self):
        self.pixel_map = _aare.GenerateMoench05PixelMap1g()

    def __call__(self, data):
        return np.take(data.view(np.uint16), self.pixel_map)
    

class Moench05TransformOld:
    #Could be moved to C++ without changing the interface
    def __init__(self):
        self.pixel_map = _aare.GenerateMoench05PixelMapOld()

    def __call__(self, data):
        return np.take(data.view(np.uint16), self.pixel_map)
    
class Moench04AnalogTransform:
    #Could be moved to C++ without changing the interface
    def __init__(self):
        self.pixel_map = _aare.GenerateMoench04AnalogPixelMap()

    def __call__(self, data):
        return np.take(data.view(np.uint16), self.pixel_map)
    
class Matterhorn02TransceiverTransform: 
    #Could be moved to C++ without changing the interface
    def __init__(self):
        self.pixel_map = _aare.GenerateMH02SingleCounterPixelMap()

    def __call__(self, data):
        return np.take(data.view(np.uint16), self.pixel_map)

class Matterhorn10Transform:
    def __init__(self, dynamic_range : int, num_counters : int):
        self.pixel_map = _aare.GenerateMatterhorn10PixelMap(dynamic_range, num_counters)
        self.dynamic_range = dynamic_range
        self.num_counters = num_counters 

    def __call__(self, data):
        if self.dynamic_range == 16:
            return np.take(data.view(np.uint16), self.pixel_map)
        elif self.dynamic_range == 8: 
            return np.take(data.view(np.uint8), self.pixel_map)
        else: #dynamic range 4
            return np.take(_aare.expand4to8bit(data.view(np.uint8)), self.pixel_map)
            
class Mythen302Transform:
    """
    Transform Mythen 302 test chip data from a buffer of bytes (uint8_t)
    to a uint32 numpy array of [64,3] representing channels and counters.
    Assumes data taken with rx_dbitlist 17 6, rx_dbitreorder 1 and Digital
    Samples = 2310 [(64x3x24)/2 + some extra]

    .. note::

        The offset is in number of bits 0-7

    """
    _n_channels = 64
    _n_counters = 3

    def __init__(self, offset=4):
        self.offset = offset

    def __call__(self, data : np.ndarray):
        """
        Transform buffer of data to a [64,3] np.ndarray of uint32. 

        Parameters
        ----------
        data : np.ndarray
            Expected dtype: uint8

        Returns
        ----------
        image : np.ndarray
            uint32 array of size  64, 3
        """
        res = _aare.decode_my302(data, self.offset)
        res = res.reshape(
            Mythen302Transform._n_channels, Mythen302Transform._n_counters
        )
        return res

#on import generate the pixel maps to avoid doing it every time
moench05 = Moench05Transform()
moench05_1g = Moench05Transform1g()
moench05_old = Moench05TransformOld()
matterhorn02 = Matterhorn02TransceiverTransform()
adc_sar_04_64to16 = AdcSar04Transform64to16()
adc_sar_05_64to16 = AdcSar05Transform64to16()
adc_sar_05_06_07_08_64to16 = AdcSar05060708Transform64to16()