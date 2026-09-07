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
