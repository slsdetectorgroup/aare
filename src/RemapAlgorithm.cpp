#include "aare/RemapAlgorithm.hpp"
#include <aare/logger.hpp>

#include <algorithm>

namespace aare::remap::algo {

/**
 * @brief Apply physical transformations to a sensor-local ROI.
 *
 * IMPORTANT:
 * Bond shifts are applied before rotation.
 * The order is intentional because bond shifts are defined in the
 * sensor's native coordinate system.
 */
inline InclusiveROI shift_rotate_roi(InclusiveROI roi,
                                     defs::SensorPixelGeometry const &pixel,
                                     defs::BondShift bond_shift,
                                     defs::Rotation rot) {
    // If there is a bond shift, translate the roi
    if (bond_shift.x != 0 || bond_shift.y != 0)
        roi = aare::inclusiveroi::geom::translate(roi, bond_shift.x,
                                                  bond_shift.y);

    // If there is a rotation given, mirror in X and Y (emulates a rotation)
    if (rot == defs::Rotation::Rotate180)
        roi = aare::inclusiveroi::geom::mirrorXY(roi, pixel.num_pix_x,
                                                 pixel.num_pix_y);

    return roi;
}

/**
 * @brief Build the strixel-to-pixel order map for one strixel group.
 *
 * The returned map describes how pixels from the user-provided ROI (normally
 * rx_roi from input file) are rearranged into the local strixel coordinate
 * system of the group.
 *
 * Coordinate systems:
 *
 * - @p roi_user is expressed in full-module coordinates.
 * - @p group_config.placement_on_sensor is expressed in the sensor-local
 *   coordinate system before applying the bond shift and sensor rotation.
 * - The group ROI is transformed by applying the bond shift first and the
 *   sensor rotation second.
 * - The returned @c effective_roi is the intersection of the user ROI
 *   (rebased into sensor-local coordinates) and the transformed group ROI.
 * - The returned @c map uses its own local strixel coordinate system.
 * - Each valid map entry contains a flattened index into the ORIGINAL
 *   user ROI, not into @c effective_roi.
 *
 * Thus:
 *
 *   map(strixel_row, strixel_col) = pixel_index_in_user_roi
 *
 * Invalid or unmapped strixel positions are initialized to -1.
 *
 * The strixel mapping is determined by the group's multiplicity and
 * modulo ordering. A reversed modulo ordering reverses the ordering
 * within each multiplicity group; it does not reverse the complete
 * strixel column ordering.
 *
 * @param group_config Configuration of the strixel group to be mapped.
 * @param pixel Native pixel geometry of the sensor to which the group
 *              is connected. Used when transforming the group ROI.
 * @param placement Location and orientation of the sensor on the module.
 * @param roi_user User-requested ROI in full-module coordinates.
 * @param bond_shift Physical bonding shift, applied before the rotation.
 *
 * @return A @c StrixelGroupToPixelMap containing:
 *         - the generated strixel-to-user-pixel order map;
 *         - the effective pixel ROI covered by the map.
 *
 * @throws std::logic_error If the group ROI width is not divisible by
 *                          the strixel multiplicity.
 */
defs::StrixelGroupToPixelMap
strixel_to_pixel_map(defs::GroupConfig const &group_config,
                     defs::SensorPixelGeometry const &pixel,
                     defs::SensorModulePlacement const &placement,
                     InclusiveROI const &roi_user, defs::BondShift bond_shift) {

    const int multiplicity = group_config.strixel.multiplicity;

    // The group must contain an integer number of strixel columns.
    const auto group_width = group_config.placement_on_sensor.width();

    if (group_width % multiplicity != 0)
        throw std::logic_error(
            "Group ROI width must be divisible by strixel multiplicity");

    const int total_strixel_columns = group_width / multiplicity;

    // Determine the ordering of strixels within each multiplicity group.
    std::vector<int> mods(multiplicity);
    std::iota(mods.begin(), mods.end(), 0);

    if (group_config.routing.mod_order == defs::ModuloOrdering::Reverse)
        std::reverse(mods.begin(), mods.end());

    // -- 1) Rebase the user ROI (rx_roi) into sensor-local coordinates
    const InclusiveROI roi_user_local =
        inclusiveroi::geom::rebaseROI(roi_user, placement.placement_on_module);
    LOG(logDEBUG)
        << "aare::remap::algo::strixel_to_pixel_map: Transformed user ROI: "
        << roi_user_local << std::endl;

    LOG(logDEBUG) << "aare::remap::algo::strixel_to_pixel_map: Group ROI "
                     "before transformation (as in global config)"
                  << group_config.placement_on_sensor << '\n';

    // -- 2) Apply the physical bond shift first, sensor rotation second.
    const InclusiveROI roi_group =
        shift_rotate_roi(group_config.placement_on_sensor, pixel, bond_shift,
                         placement.rotation);

    LOG(logDEBUG) << "aare::remap::algo::strixel_to_pixel_map: Group ROI after "
                     "transformation (as in local transformation) "
                  << roi_group << '\n';

    // -- 3) Compute effective ROI = intersection( roi_user, roi_group )
    // Only pixels covered by both the user ROI and the transformed group
    // contribute to this map.
    const InclusiveROI effective_roi =
        inclusiveroi::geom::intersect(roi_user_local, roi_group);

    // If ROIs don't intersect, return empty
    if (effective_roi.xmax < effective_roi.xmin ||
        effective_roi.ymax < effective_roi.ymin) {
        return {{}, InclusiveROI::emptyROI()};
    }

    LOG(logDEBUG) << "aare::remap::algo::strixel_to_pixel_map: Result of "
                     "intersecting ROIs "
                  << effective_roi << '\n';

    /******************************
     * Core of the algorithm
     *
     * Local lambda:
     * Convert a sensor-local pixel coordinate into the corresponding
     * local strixel coordinate.
     * (Could be a separate function if preferred.)
     ******************************/
    auto pixel_to_strixel = [&](int x, int y) {
        const int dx = x - roi_group.xmin;
        const int dy = y - roi_group.ymin;

        const int mod = dx % multiplicity;
        const int col = dx / multiplicity;
        const int row = dy * multiplicity + mods[mod];

        return std::pair<int, int>{row, col};
    };

    //-- 4) Determine the range of strixel coordinates touched by the effective
    // ROI.
    int min_row = std::numeric_limits<int>::max();
    int max_row = std::numeric_limits<int>::min();
    int min_col = std::numeric_limits<int>::max();
    int max_col = std::numeric_limits<int>::min();

    for (int y = effective_roi.ymin; y <= effective_roi.ymax; ++y) {
        for (int x = effective_roi.xmin; x <= effective_roi.xmax; ++x) {

            auto [row, col] = pixel_to_strixel(x, y);

            if (col >= total_strixel_columns)
                continue;

            min_row = std::min(min_row, row);
            max_row = std::max(max_row, row);
            min_col = std::min(min_col, col);
            max_col = std::max(max_col, col);
        }
    }

    if (min_row > max_row) {
        return {{}, effective_roi}; // nothing mapped            
    }

    // Now from the found bounds of the strixel grid, we define the space to
    // allocate for the order map
    const int nrows = max_row - min_row + 1;
    const int ncols = max_col - min_col + 1;

    // And allocate
    aare::NDArray<ssize_t, 2> map({nrows, ncols}, -1);

    LOG(logDEBUG)
        << "aare::remap::algo::strixel_to_pixel_map: Resulting strixel grid: ("
        << map.shape(0) << ", " << map.shape(1) << ")" << '\n';

    // -- 5) Populate the strixel-to-user-pixel map.
    for (int y = effective_roi.ymin; y <= effective_roi.ymax; ++y) {
        for (int x = effective_roi.xmin; x <= effective_roi.xmax; ++x) {

            auto [row, col] = pixel_to_strixel(x, y);

            const int map_col = col - min_col;
            const int map_row = row - min_row;

            // index into !!!ORIGINAL USER ROI GRID!!! (use local
            // coordinates)
            const ssize_t user_pixel =
                static_cast<ssize_t>(y - roi_user_local.ymin) *
                    roi_user_local.width() +
                (x - roi_user_local.xmin);

            map(map_row, map_col) = user_pixel;
        }
    }

    return {map, effective_roi};
};

std::vector<defs::StrixelGroupToPixelMap>
strixel_to_pixel_maps(defs::SensorConfig const &sensor_config,
                      defs::SensorModulePlacement const &placement,
                      InclusiveROI const &roi_user,
                      defs::BondShift bond_shift) {

    std::vector<defs::StrixelGroupToPixelMap> maps;
    maps.reserve(sensor_config.group_configs.size());

    for (size_t i = 0; i < sensor_config.group_configs.size(); ++i) {
        maps.emplace_back(strixel_to_pixel_map(sensor_config.group_configs[i],
                                               sensor_config.pixel, placement,
                                               roi_user, bond_shift));
    }

    return maps;
}

} // namespace aare::remap::algo