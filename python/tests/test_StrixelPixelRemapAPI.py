import pytest 

import numpy as np 

from aare import strixelremap
from aare import ROI

def test_roimappings(): 
    """ Test exclusive to inclusive roi mapping and vice versa """

    roi = ROI(0, 10, 0, 5)

    inclusive_roi = strixelremap.toInclusiveROI(roi) 

    assert inclusive_roi.xmin == 0
    assert inclusive_roi.xmax == 9
    assert inclusive_roi.ymin == 0
    assert inclusive_roi.ymax == 4

    exclusive_roi = strixelremap.toHalfopenROI(inclusive_roi)

    assert exclusive_roi.xmin == 0
    assert exclusive_roi.xmax == 10
    assert exclusive_roi.ymin == 0
    assert exclusive_roi.ymax == 5

def test_emptyROI(): 
    """ Test creation of an empty ROI """

    empty_roi = strixelremap.InclusiveROI.emptyROI() 

    assert empty_roi.xmin == 0
    assert empty_roi.xmax == -1
    assert empty_roi.ymin == 0
    assert empty_roi.ymax == -1

def test_customSensorConfiguration(): 
    """ Test that a custom sensor configuration can be created and used to remap strixel pixels """

    my_strixel_group = strixelremap.GroupStrixelGeometry(multiplicity=2, pitch_um=25.0)

    assert my_strixel_group.multiplicity == 2
    assert my_strixel_group.pitch_um == 25.0

    my_sensor_geometry = strixelremap.SensorPixelGeometry(num_pix_x = 10, num_pix_y = 5)

    assert my_sensor_geometry.num_pix_x == 10
    assert my_sensor_geometry.num_pix_y == 5
    assert my_sensor_geometry.guardring == strixelremap.Guardring(0,0)

    my_group_config = strixelremap.GroupConfig(strixel = my_strixel_group, routing = strixelremap.ModuloOrdering.Forward,placement_on_sensor = strixelremap.InclusiveROI(0,9,0,4))
    
    my_sensor_config = strixelremap.SensorConfig(sensor_geometry = my_sensor_geometry, group_configs = [my_group_config])

    # rebase 
    user_roi = strixelremap.InclusiveROI(strixelremap.Chip1.placement_on_module.xmin + 0, strixelremap.Chip1.placement_on_module.xmin + 9, strixelremap.Chip1.placement_on_module.ymin + 0, strixelremap.Chip1.placement_on_module.ymin + 4)

    strixelpixelmap = strixelremap.strixel_to_pixel_maps(sensor_config = my_sensor_config, placement = strixelremap.Chip1, user_roi = user_roi)

    assert len(strixelpixelmap) == 1

    assert strixelpixelmap[0].effective_roi == my_group_config.placement_on_sensor

    map = strixelpixelmap[0].map
    assert map.shape == (10 ,5)

    assert np.array_equal(map, np.array([[0, 2, 4, 6, 8], [1, 3, 5, 7, 9], [10, 12, 14, 16, 18], [11, 13, 15, 17, 19], [20, 22, 24, 26, 28], [21, 23, 25, 27, 29], [30, 32, 34, 36, 38], [31, 33, 35, 37, 39], [40, 42, 44, 46, 48], [41, 43, 45, 47, 49]])) 

def test_predefinedRemap(): 
    """ Test predefined maps API """

    user_roi = strixelremap.InclusiveROI(strixelremap.Chip1.placement_on_module.xmin + 5, strixelremap.Chip1.placement_on_module.xmin + 9, strixelremap.Chip1.placement_on_module.ymin + 5, strixelremap.Chip1.placement_on_module.ymin + 7)
    strixelpixelmap = strixelremap.jungfrau_tew_singlechip_25um_strixel_map(user_roi = user_roi, placement = strixelremap.Chip1)

    assert strixelpixelmap.map.shape == (9, 2)

    assert np.array_equal(strixelpixelmap.map, np.array([[-1, 2], [0, 3], [1, 4], [-1, 7], [5,8], [6,9], [-1,12], [10,13], [11,14]])) 

    input_data = np.array([[1,2,3,4,5],[1,2,3,4,5], [1,2,3,4,5]]).astype(np.uint16)

    order_map = strixelpixelmap.map

    output = np.empty(order_map.shape, dtype=input_data.dtype)

    strixelremap.apply_remap(input_data, order_map, output)
    assert np.array_equal(output, np.array([[0, 3], [1,4], [2,5], [0, 3], [1,4], [2,5], [0, 3], [1,4], [2,5]]))


def test_apply_remap():
    """ Apply remap throws upon invalid input data type """

    strixelpixelmap = strixelremap.jungfrau_ilgad_singlechip_25um_strixel_map(user_roi = strixelremap.Chip1.placement_on_module, placement = strixelremap.Chip1) 

    order_map = strixelpixelmap.map

    user_roi_height = strixelremap.Chip1.placement_on_module.height
    user_roi_width = strixelremap.Chip1.placement_on_module.width

    data = np.random.rand(user_roi_height, user_roi_width).astype(np.float64)

    output = np.empty(order_map.shape, dtype=data.dtype)

    with pytest.raises(RuntimeError):
        strixelremap.apply_remap(data, order_map, output)
