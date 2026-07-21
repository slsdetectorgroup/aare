#pragma once

#include "aare/InclusiveROI.hpp"
#include "aare/NDArray.hpp"

#include <vector>

namespace aare::remap::defs {

/**
 * Physical orientation of the sensor on the module.
 *
 * This transformation describes how the entire sensor-ASIC assembly (pixel grid
 * and strixel routing together) is mounted with respect to the module
 * coordinate system.
 *
 * Identity:
 *   Sensor-ASIC assembly is mounted in its nominal orientation (wire bond pad
 *   aligns with bottom of HDI).
 *
 * Rotate180:
 *   Sensor-ASIC assembly is rotated by 180° before wire bonding.
 *   This is equivalent to mirroring both x and y coordinates.
 */
enum class Rotation : int { Identity = 0, Rotate180 = 1 };

/**
 * Ordering of pixel-to-strixel routing within each multiplicity group.
 *
 * Forward:
 *   Pixels are assigned to strixel rows in increasing column order.
 *
 * Reverse:
 *   The ordering within each multiplicity group is reversed.
 *
 * Example (multiplicity = 3):
 *
 *   Forward : pixel columns [0,1,2] -> strixel rows [0,1,2]
 *   Reverse : pixel columns [0,1,2] -> strixel rows [2,1,0]
 *
 * This affects only the ordering inside each multiplicity group.
 * It does not mirror or reorder the groups themselves.
 */
enum class ModuloOrdering { Forward, Reverse };

struct Guardring {
    int x;
    int y;
};

struct BondShift {
    int x = 0;
    int y = 0;
};

/**
 * Describes the native ASIC pixel grid of a sensor.
 *
 * This geometry is shared by all strixel groups on the sensor and defines
 * the coordinate system in which remapping is performed.
 */
struct SensorPixelGeometry {
    int num_pix_x;
    int num_pix_y;
    Guardring guardring;
};

/**
 * Describes the strixel geometry of a single remapping group.
 *
 * Pixel rows are multiplied by `multiplicity`, resulting
 * in an effective strixel pitch of `pitch_um`, and pixel columns are divided by
 * `multiplicity`.
 */
struct GroupStrixelGeometry {
    int multiplicity;
    double pitch_um;
};

/**
 * Describes the routing between ASIC pixel columns and strixel rows
 * within a remapping group.
 *
 * The modulo ordering specifies whether the pixels belonging to each
 * multiplicity group are mapped in forward or reversed order.
 */
struct GroupRouting {
    ModuloOrdering mod_order = ModuloOrdering::Forward;
};

/**
 * Configuration of a single contiguous strixel group on a sensor.
 *
 * A group is characterized by
 *   - its strixel geometry,
 *   - its routing pattern, and
 *   - its location within the native sensor pixel grid.
 */
struct GroupConfig {
    GroupStrixelGeometry strixel;
    GroupRouting routing;

    /// Group bounds in native sensor pixel coordinates.
    InclusiveROI placement_on_sensor;
};

/**
 * Complete description of a sensor.
 *
 * A sensor consists of a single native pixel geometry together with one or
 * more remapping groups that partition the sensor into regions with different
 * strixel geometries and/or routing.
 */
struct SensorConfig {
    SensorPixelGeometry pixel;
    std::vector<GroupConfig> group_configs;
};

/**
 * Describes the placement of a sensor within the module.
 *
 * Specifies where the sensor is located in module coordinates and how it is
 * physically oriented with respect to the module reference frame.
 */
struct SensorModulePlacement {
    /// Sensor bounds in module coordinates.
    InclusiveROI placement_on_module;

    /// Physical orientation of the mounted sensor.
    Rotation rotation;
};

/**
 * Describes the remapping result plus the metadata that was used to create the
 * result.
 */
struct StrixelGroupToPixelMap {
    NDArray<ssize_t, 2> map;

    // Configuration used to generate this map
    GroupConfig group_config;

    // Sensor geometry used by the algorithm
    SensorPixelGeometry pixel;

    // Effective group ROI after bond shift and rotation
    InclusiveROI effective_group_roi;

    // Region actually covered by this map (intersection with user ROI)
    InclusiveROI effective_roi;
};

} // namespace aare::remap::defs