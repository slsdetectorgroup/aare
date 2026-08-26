#pragma once

#include "aare/InclusiveROI.hpp"
#include "aare/NDArray.hpp"

#include <vector>

namespace aare::remap::defs {

/**
 * @brief Physical orientation of the sensor on the module.
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
 * @brief Ordering of pixel-to-strixel routing within each multiplicity group.
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

/// @brief Describes the physical guardring around a sensor to protect the ASIC
/// from high fluxes.
struct Guardring {
    /// @brief ring width.
    int x{};
    /// @brief ring height.
    int y{};
};

struct BondShift {
    /// @brief Bonding shift in x direction
    int x{};
    /// @brief Bonding shift in y direction
    int y{};
};

/**
 * @brief Describes the native ASIC pixel grid of a sensor.
 *
 * This geometry is shared by all strixel groups on the sensor and defines
 * the coordinate system in which remapping is performed.
 */
struct SensorPixelGeometry {
    /// @brief Number of pixels in x direction (columns)
    int num_pix_x;
    /// @brief Number of pixels in y direction (rows)
    int num_pix_y;
    /// @brief guardring around sensor
    Guardring guardring;
};

/**
 * @brief Describes the strixel geometry of a single remapping group.
 */
struct GroupStrixelGeometry {
    /// @brief Number of pixels a strixel covers.
    int multiplicity;
    /// @brief Effective strixel pitch [µm]
    double pitch_um;
};

/**
 * @brief Describes the routing between ASIC pixel columns and strixel rows
 * within a remapping group.
 *
 * The modulo ordering specifies whether the pixels belonging to each
 * multiplicity group are mapped in forward or reversed order.
 */
struct GroupRouting {
    ModuloOrdering mod_order = ModuloOrdering::Forward;
};

/**
 * @brief Configuration of a single contiguous strixel group on a sensor.
 */
struct GroupConfig {
    /// @brief strixel geometry
    GroupStrixelGeometry strixel;

    /// @brief pixel-to-strixel routing pattern
    GroupRouting routing;

    /// @brief location on sensor (given in sensor's native coordinates)
    InclusiveROI placement_on_sensor;
};

/**
 * @brief Complete description of a sensor.
 */
struct SensorConfig {
    /// @brief pixel geometry of the sensor
    SensorPixelGeometry pixel;

    /// @brief strixel group configurations partitions the sensor into regions
    /// with different strixel geometries and/or routing
    std::vector<GroupConfig> group_configs;
};

/**
 * @brief Describes the placement of a sensor within the module.
 */
struct SensorModulePlacement {
    /// @brief Sensor bounds in module coordinates.
    InclusiveROI placement_on_module;

    /// @brief Physical orientation of the mounted sensor with respect to the
    /// module reference frame.
    Rotation rotation;
};

/**
 * @brief Result of remapping one strixel group onto the pixel grid.
 *
 * The order map defines a local strixel-grid coordinate system. For each
 * map position (row, col), map(row, col) contains the flattened pixel index
 * of the corresponding source pixel in the user-provided input ROI for strixel
 * defined at (row, col).
 *
 * For a valid entry:
 *
 *   map(row, col) = dy * roi_user.width() + dx
 *
 * where (dx, dy) is the pixel position in the coordinate system of the
 * original user-provided ROI.
 *
 * NOTE: The map coordinates (row, col) are local to this strixel group and
 * are geometrically associated with effective_roi. The stored pixel index,
 * however, is flattened with respect to the original user-provided ROI,
 * not effective_roi. (I.e.: The map provides a local strixel grid mapped to
 * the correct corresponding pixel indices in the original user grid, and
 * effective_roi is the ROI the algorithm used for remapping.)
 *
 * An entry of -1 indicates that the corresponding strixel position has no
 * valid source pixel.
 */
struct StrixelGroupToPixelMap {
    /**
     * @brief Strixel-to-pixel order map.
     *
     * The indices (row, col) define the local coordinate system of the
     * strixel group. Each value is a flattened index into the original
     * user-provided ROI.
     */
    NDArray<ssize_t, 2> map;

    /**
     * @brief Effective pixel ROI covered by this map.
     *
     * This is the transformed group ROI after applying the bond shift and
     * sensor rotation and intersecting it with the user-provided ROI.
     *
     * The local coordinate system of `map` is aligned with this ROI.
     */
    InclusiveROI effective_roi;
};

} // namespace aare::remap::defs