import numpy as np
from . import _aare


class Moench05Transform:
    #Could be moved to C++ without changing the interface
    def __init__(self):
        print('map created')
        self.pixel_map = _aare.GenerateMoench05PixelMap()

    def __call__(self, data):
        return np.take(data.view(np.uint16), self.pixel_map)


moench05 = Moench05Transform()
