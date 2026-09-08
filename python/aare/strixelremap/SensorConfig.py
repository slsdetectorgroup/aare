from .._aare import strixelremap 


def SensorConfig(sensor_geometry, group_configs):
    """ 
    Helper function to create a sensor configuration from a sensor geometry and a list of group configurations

    Args:
        sensor_geometry (SensorPixelGeometry): The sensor geometry
        group_configs (list[GroupConfig]): The list of group configurations
    """

    if(len(group_configs) == 0):
        raise ValueError("group_configs must contain at least one group configuration")
    
    N = len(group_configs)

    if N > 4: 
        raise ValueError("sensor configs with more than 4 groups are not bound in Python")
    
    sensor_config_cls = getattr(strixelremap, f"SensorConfig_{N}PixelGroups")
    return sensor_config_cls(sensor_geometry, group_configs)


# helper function for easier documentation of templated strixel_to_pixel_maps
def strixel_to_pixel_maps(sensor_config : SensorConfig, placement : strixelremap.SensorModulePlacement, user_roi : strixelremap.InclusiveROI, bond_shift : strixelremap.BondShift = strixelremap.BondShift(0, 0)) -> list[strixelremap.StrixelGroupToPixelMap]: 
    """ 
    Creates a StrixeltoPixelMap for all GroupConfigs in a SensorConfig

    Parameters
    ----------

    sensor_config : SensorConfig
        Configuration of the sensor, including all configurations of the strixel groups.
    placement : SensorModulePlacement
        Placement and orientation of the sensor on the module.
    user_roi : InclusiveROI
        User-defined region of interest. (in global module coordinates)
    bond_shift : BondShift, optional
        Shift applied to the bond positions. Default is (0, 0).

    Returns
    -------

    list[StrixelGroupToPixelMap]
        map(row, col) contains the flattened pixel index of the corresponding source pixel in the user-provided input ROI for strixel defined at (row, col).
        An entry of -1 indicates that the corresponding strixel position has no valid source pixel.
    """

    return strixelremap.strixel_to_pixel_maps_pybindfunc(sensor_config, placement, user_roi, bond_shift)