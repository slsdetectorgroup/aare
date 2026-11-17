



import numpy as np
import pyperf

from aare import strixelremap

def numpy_take(input, order_map, output): 
    """Remap input array to output array using order_map with numpy.take"""
    not_mapped = order_map == -1
    np.take(input, order_map, out=output)
    output[not_mapped] = 0

def benchmark_remap():
    
    strixelpixelmap = strixelremap.jungfrau_ilgad_singlechip_25um_strixel_map(user_roi = strixelremap.Chip1.placement_on_module, placement = strixelremap.Chip1) 

    order_map = strixelpixelmap.map

    user_roi_height = strixelremap.Chip1.placement_on_module.height
    user_roi_width = strixelremap.Chip1.placement_on_module.width

    data = np.random.randint(0, 2**16, size=(user_roi_height, user_roi_width)).astype(np.uint16)

    output = np.empty(order_map.shape, dtype=data.dtype)

    runner = pyperf.Runner()

    runner.bench_func("apply_remap", strixelremap.apply_remap, data, order_map, output)

    runner.bench_func("numpy_take", numpy_take, data, order_map, output)

if  __name__ == "__main__": 
    benchmark_remap()
