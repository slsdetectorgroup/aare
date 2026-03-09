#pragma once

#include "aare/InclusiveROI.hpp"
#include "aare/NDArray.hpp"

#include <vector>

namespace aare::remap::defs {

// Orientation of the sensor with respect to the ASICs / the HDI
// Normal means lower part of sensor (as in GDS) aligns with lower part of HDI
enum class Rotation : int { Normal = 0, Inverse = 1 };

struct Guardring {
    int x;
    int y;
};

struct BondShift {
    int x = 0;
    int y = 0;
};

// Define the native pixel grid of the sensor (as connected to ASIC)
struct SensorPixelGeometry {
    int num_pix_x;
    int num_pix_y;
    Guardring guardring;
};

// Define the strixel grid of the sensor
struct SensorStrixelGeometry {
    int multiplicity;
    double pitch_um;
};

// Fully characterize a strixel group (contiguous area that will be remapped)
struct SensorGroupConfig {
    SensorPixelGeometry pixel;
    SensorStrixelGeometry strixel;
    InclusiveROI placement_on_sensor; // location of the group in the sensor
                                      // coordinate system (or reduced roi)
};

// Combine multiple strixel areas that make up a single sensor
struct SensorConfig {
    std::vector<SensorGroupConfig> group_configs;
};

// Define location and orientation of a sensor on the module
struct SensorPlacement {
    InclusiveROI placement_on_module; // location of the sensor in full-module
                                      // coordinate system
    Rotation rotation;
};

// Possibly, this may need to be moved to a dedicated "remapping" class
struct StrixelGroupToPixelMap {
    int multiplicity;
    double pitch_um;
    InclusiveROI placement_on_sensor; // location of the group in the sensor
                                      // coordinate system (or reduced roi)
    NDArray<ssize_t, 2> map;
};

} // namespace aare::remap::defs