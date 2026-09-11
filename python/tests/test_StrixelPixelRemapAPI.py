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


def test_format(): 
    """ Test string representations """

    inclusive_roi = strixelremap.InclusiveROI(0, 9, 0, 4)

    assert str(inclusive_roi) == "InclusiveROI(xmin=0, xmax=9, ymin=0, ymax=4)"

    jungfrau_pixel_geometry = strixelremap.SingleChipMP_TEW_pix

    assert str(jungfrau_pixel_geometry) == "SensorPixelGeometry{cols x rows: 256 x 256, guardring: {x = 0, y = 0}}"

    strixel_group = strixelremap.StrxP25

    assert str(strixel_group) == "GroupStrixelGeometry{multiplicity: 3, pitch_um: 25}"

    jungfrau_group_config = strixelremap.SingleChipMP_TEW_P25

    assert str(jungfrau_group_config) == "GroupConfig{strixel_group: {multiplicity: 3, pitch_um: 25}, routing: {Forward}, placement_on_sensor: {xmin=1, xmax=255, ymin=0, ymax=63}}"

    sensor_placement = strixelremap.Chip1

    assert str(sensor_placement) == "SensorModulePlacement{placement_on_module: {xmin=256, xmax=511, ymin=0, ymax=255}, rotation: Identity}"

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

    strixelpixelmap = strixelremap.StrixelPixelMap(sensor_config = my_sensor_config, placement = strixelremap.Chip1) 

    strixelpixelmap.calculate_map(strixelremap.toHalfopenROI(user_roi))

    group_maps = strixelpixelmap.group_maps

    assert len(group_maps) == 1

    assert group_maps[0].effective_roi == my_group_config.placement_on_sensor

    map = group_maps[0].map
    assert map.shape == (10 ,5)

    assert np.array_equal(map, np.array([[0, 2, 4, 6, 8], [1, 3, 5, 7, 9], [10, 12, 14, 16, 18], [11, 13, 15, 17, 19], [20, 22, 24, 26, 28], [21, 23, 25, 27, 29], [30, 32, 34, 36, 38], [31, 33, 35, 37, 39], [40, 42, 44, 46, 48], [41, 43, 45, 47, 49]])) 

    input = np.arange(50).reshape((5,10)).astype(np.uint16)

    remapped_result = strixelpixelmap(input)[0]

    assert remapped_result.shape == (10,5)

    assert np.array_equal(remapped_result, np.array([[0, 2, 4, 6, 8], [1, 3, 5, 7, 9], [10, 12, 14, 16, 18], [11, 13, 15, 17, 19], [20, 22, 24, 26, 28], [21, 23, 25, 27, 29], [30, 32, 34, 36, 38], [31, 33, 35, 37, 39], [40, 42, 44, 46, 48], [41, 43, 45, 47, 49]]))

    # check output call operator with preallocated output array
    output = np.empty(map.shape, dtype=input.dtype)

    strixelpixelmap(input, [output])

    assert np.array_equal(output, np.array([[0, 2, 4, 6, 8], [1, 3, 5, 7, 9], [10, 12, 14, 16, 18], [11, 13, 15, 17, 19], [20, 22, 24, 26, 28], [21, 23, 25, 27, 29], [30, 32, 34, 36, 38], [31, 33, 35, 37, 39], [40, 42, 44, 46, 48], [41, 43, 45, 47, 49]]))

def test_predefined_iLGAD_singlechip(): 
    """ Test predefined Junfrau iLGAD strixel pixel remap """

    inclusive_user_roi = strixelremap.InclusiveROI(strixelremap.Chip1.placement_on_module.xmin + 11, strixelremap.Chip1.placement_on_module.xmin + 15, strixelremap.Chip1.placement_on_module.ymin + 10, strixelremap.Chip1.placement_on_module.ymin + 12)

    exclusive_user_roi = strixelremap.toHalfopenROI(inclusive_user_roi)

    strixelpixelmap = strixelremap.Jungfrau_iLGAD_StrixelPixelMap(module_placement = strixelremap.Chip1)

    strixelpixelmap.calculate_map(exclusive_user_roi)

    group_maps = strixelpixelmap.group_maps

    assert len(group_maps) == 3

    assert group_maps[0].map.shape == (9, 2)

    assert group_maps[1].map.shape == (0, 0)

    assert group_maps[2].map.shape == (0, 0)

    group_map_0 = group_maps[0].map

    assert np.array_equal(group_map_0, np.array([[-1, 2], [0, 3], [1, 4], [-1, 7], [5,8], [6,9], [-1,12], [10,13], [11,14]])) 

    input_data = np.array([[1,2,3,4,5],[1,2,3,4,5], [1,2,3,4,5]]).astype(np.uint16)

    output = strixelpixelmap(input_data)

    assert len(output) == 3 
   
    assert np.array_equal(output[0], np.array([[0, 3], [1,4], [2,5], [0, 3], [1,4], [2,5], [0, 3], [1,4], [2,5]]))

    assert output[1].size() == 0

    assert output[2].size() == 0


def test_predefined_iLGAD_quad_remap():
    """ Test predefined Junfrau iLGAD quad strixel pixel remap """

    # TODO combine map with one empty 
    inclusive_user_roi = strixelremap.InclusiveROI(strixelremap.Quad.placement_on_module.xmin + 11, strixelremap.Quad.placement_on_module.xmin + 15, strixelremap.Quad.placement_on_module.ymin + 9, strixelremap.Quad.placement_on_module.ymin + 11)

    exclusive_user_roi = strixelremap.toHalfopenROI(inclusive_user_roi)

    strixelpixelmap = strixelremap.Jungfrau_iLGAD_Quad_StrixelPixelMap()

    strixelpixelmap.calculate_map(exclusive_user_roi)

    group_maps = strixelpixelmap.group_maps

    assert len(group_maps) == 1 

    assert group_maps[0].map.shape == (9, 2)



