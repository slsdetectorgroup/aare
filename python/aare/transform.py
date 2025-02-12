import numpy as np
from . import _aare


class AdcSar04Transform64to16:
    def __call__(self, data):
        return _aare.adc_sar_04_decode64to16(data)

class AdcSar05Transform64to16:
    def __call__(self, data):
        return _aare.adc_sar_05_decode64to16(data)

class Moench05Transform:
    #Could be moved to C++ without changing the interface
    def __init__(self):
        self.pixel_map = _aare.GenerateMoench05PixelMap()

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


class Matterhorn02Transform:
    def __init__(self):
        self.pixel_map = _aare.GenerateMH02FourCounterPixelMap()

    def __call__(self, data):
        counters = int(data.size / 48**2 / 2)
        if counters == 1:
            return np.take(data.view(np.uint16), self.pixel_map[0])
        else:
            return np.take(data.view(np.uint16), self.pixel_map[0:counters])


#on import generate the pixel maps to avoid doing it every time
moench05 = Moench05Transform()
moench05_1g = Moench05Transform1g()
moench05_old = Moench05TransformOld()
matterhorn02 = Matterhorn02Transform()
adc_sar_04_64to16 = AdcSar04Transform64to16()
adc_sar_05_64to16 = AdcSar05Transform64to16()