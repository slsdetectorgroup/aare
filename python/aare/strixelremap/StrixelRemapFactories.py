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


def StrixelPixelMap(sensor_config , placement : strixelremap.SensorModulePlacement, bond_shift : strixelremap.BondShift = strixelremap.BondShift(0, 0)): 
    """ 
    Helper function to create a StrixelPixelMap from a SensorConfig

    Args:
        sensor_config (SensorConfig): The sensor configuration
        placement (SensorModulePlacement): The placement of the sensor on the module
        bond_shift (BondShift, optional): The bond shift to apply to the sensor. Defaults to BondShift(0, 0).
    Returns:
        StrixelPixelMap: The strixel pixel map object for the given sensor configuration and placement
    """

    N = len(sensor_config.group_configs)

    sensor_config_cls = getattr(strixelremap, f"StrixelPixelMap_{N}Groups_{N}Maps")

    return sensor_config_cls(sensor_config, placement, bond_shift)
