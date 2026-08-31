#include "aare/StrixelPixelRemapAlgorithm.hpp"
#include <aare/logger.hpp>

#include <algorithm>

namespace aare::remap::algo {

defs::StrixelGroupToPixelMap
strixel_to_pixel_map(defs::GroupConfig const &group_config,
                     defs::SensorPixelGeometry const &pixel,
                     defs::SensorModulePlacement const &placement,
                     InclusiveROI const &roi_user, defs::BondShift bond_shift) {

    const int multiplicity = group_config.strixel.multiplicity;

    // Defensive check to be sure misconfiguration is avoided
    if (multiplicity <= 0)
        throw std::logic_error("Strixel multiplicity must be positive");

    // The group must contain an integer number of strixel columns.
    const auto group_width = group_config.placement_on_sensor.width();

    if (group_width % multiplicity != 0)
        throw std::logic_error(
            "Group ROI width must be divisible by strixel multiplicity");

    // const int total_strixel_columns = group_width / multiplicity;

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
    const InclusiveROI roi_group = detail::update_pixel_group_placement(
        group_config.placement_on_sensor, pixel, bond_shift,
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
        LOG(logWARNING)
            << "User-supplied ROI does not intersect with configured "
               "strixel ROI, returned map is empty!\n";
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
    //
    // Since effective_roi is contained in roi_group:
    //   dx = x - roi_group.xmin >= 0
    //   dy = y - roi_group.ymin >= 0
    //
    // The strixel column is dx / multiplicity.
    // Each pixel row maps onto a complete block of `multiplicity`
    // strixel rows, regardless of the modulo ordering.
    const int min_col = (effective_roi.xmin - roi_group.xmin) / multiplicity;

    const int max_col = (effective_roi.xmax - roi_group.xmin) / multiplicity;

    const int min_row = (effective_roi.ymin - roi_group.ymin) * multiplicity;

    // Catch the first row that is out of bounds (next multiplicity group) and
    // calculate -1
    const int max_row =
        (effective_roi.ymax - roi_group.ymin + 1) * multiplicity - 1;

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

            // index into !!!ORIGINAL USER ROI GRID!!!
            const ssize_t user_pixel =
                static_cast<ssize_t>(y - roi_user_local.ymin) *
                    roi_user_local.width() +
                (x - roi_user_local.xmin);

            map(map_row, map_col) = user_pixel;
        }
    }

    return {map, effective_roi};
}

} // namespace aare::remap::algo