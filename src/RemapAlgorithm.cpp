#include "aare/RemapAlgorithm.hpp"

#include <algorithm>

namespace aare::remap::algo {

// Is it better to pass defs::SensorGroupConfig const& and return a copy?
// Better not to, use shift_rotate_roi instead
void apply_rotation_shift(defs::GroupConfig &cfg,
                          defs::SensorPixelGeometry const &pixel,
                          defs::BondShift bond_shift, defs::Rotation rot) {
    // Apply physical transforms
    if (bond_shift.x != 0 || bond_shift.y != 0)
        cfg.placement_on_sensor = aare::inclusiveroi::geom::translate(
            cfg.placement_on_sensor, bond_shift.x, bond_shift.y);

    if (rot == defs::Rotation::Rotate180)
        cfg.placement_on_sensor = aare::inclusiveroi::geom::mirrorXY(
            cfg.placement_on_sensor, pixel.num_pix_x, pixel.num_pix_y);
}

/**
 * Apply physical transformations to a sensor-local ROI.
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

defs::StrixelGroupToPixelMap
strixel_to_pixel_map(defs::GroupConfig const &group_config,
                     defs::SensorPixelGeometry const &pixel,
                     defs::SensorPlacement const &placement,
                     InclusiveROI const &roi_user, defs::BondShift bond_shift) {

    int multiplicity = group_config.strixel.multiplicity;
    double pitch = group_config.strixel.pitch_um;
    // defs::Rotation rot = placement.rotation;

    // Helper to make sure that we work with a correct number of strixel columns
    // (i.e. that we do not map pixel columns if the ncols in ASIC pixel
    // coordinates is not a multiple of strixel ncols)
    if (group_config.placement_on_sensor.width() % multiplicity != 0)
        throw std::logic_error("Group ROI width not divisible by multiplicity");

    const int tot_ncols_strx =
        group_config.placement_on_sensor.width() / multiplicity;

    // Define mod ordering
    std::vector<int> mods(multiplicity);
    std::iota(mods.begin(), mods.end(), 0);

    if (group_config.routing.mod_order == defs::ColumnModOrdering::Reverse)
        std::reverse(mods.begin(), mods.end());

    // -- 1) Transform user roi (rx_roi) into sensor-local coordinates
    InclusiveROI roi_user_local =
        inclusiveroi::geom::rebaseROI(roi_user, placement.placement_on_module);
    std::cout << "Transformed user ROI: " << roi_user_local << std::endl;

    // DEBUG
    std::cout << "DEBUG: Group ROI before transformation (as in global config)"
              << group_config.placement_on_sensor << '\n';

    // -- 2) Apply transforms (if necessary)
    // -- 2a) bond_shift
    // -- 2b) rotation
    InclusiveROI roi_group =
        shift_rotate_roi(group_config.placement_on_sensor, pixel, bond_shift,
                         placement.rotation);

    // DEBUG
    std::cout
        << "DEBUG: Group ROI after transformation (as in local transformation) "
        << roi_group << '\n';

    // -- 3) Compute effective ROI = intersection( roi_user, roi_group )
    InclusiveROI eff = inclusiveroi::geom::intersect(roi_user_local, roi_group);
    if (eff.xmax < eff.xmin || eff.ymax < eff.ymin) {
        return {-1, 0.0, InclusiveROI::emptyROI(), {}}; // empty
    }

    // DEBUG
    std::cout << "DEBUG: Result of intersecting ROIs " << eff << '\n';

    //-- 4) Determine min/max row/col of strixel grid before allocating
    //      (This may vary from the native grid of the group because of ROI
    //      intersection.)
    int min_row_strx = std::numeric_limits<int>::max();
    int max_row_strx = std::numeric_limits<int>::min();
    int min_col_strx = std::numeric_limits<int>::max();
    int max_col_strx = std::numeric_limits<int>::min();

    for (int y = eff.ymin; y <= eff.ymax; ++y) {
        for (int x = eff.xmin; x <= eff.xmax; ++x) {

            const int dx = x - roi_group.xmin;
            const int dy = (y - roi_group.ymin);

            const int m = dx % multiplicity;
            const int col_strx = dx / multiplicity;
            const int row_strx = dy * multiplicity + mods[m];

            if (col_strx < 0 || row_strx < 0)
                continue;
            if (col_strx >= tot_ncols_strx)
                continue;

            min_row_strx = std::min(min_row_strx, row_strx);
            max_row_strx = std::max(max_row_strx, row_strx);
            min_col_strx = std::min(min_col_strx, col_strx);
            max_col_strx = std::max(max_col_strx, col_strx);
        }
    }

    if (min_row_strx > max_row_strx) {
        return {multiplicity, pitch, eff, {}}; // nothing mapped
    }

    // Now from the found bounds of the strixel grid, we define the space to
    // allocate for the order map
    const int nrows_strx = max_row_strx - min_row_strx + 1;
    const int ncols_strx = max_col_strx - min_col_strx + 1;

    // And allocate
    aare::NDArray<ssize_t, 2> map({nrows_strx, ncols_strx}, -1);

    // DEBUG
    std::cout << "DEBUG: Resulting strixel grid: (" << map.shape(0) << ", "
              << map.shape(1) << ")" << '\n';

    // -- 5) For each ASIC pixel in eff ROI, compute remapped (row,col) in group
    //       local coordinates
    for (int y = eff.ymin; y <= eff.ymax; ++y) {
        for (int x = eff.xmin; x <= eff.xmax; ++x) {

            const int dx = x - roi_group.xmin;
            const int dy = (y - roi_group.ymin);

            const int m = dx % multiplicity; // since eff is intersected with
                                             // roi_group, dx >= 0, so no issue
            const int col_strx = dx / multiplicity;
            const int row_strx = dy * multiplicity + mods[m];

            if (col_strx < min_col_strx || row_strx < min_row_strx)
                continue;

            const int cstrx = col_strx - min_col_strx;
            const int rstrx = row_strx - min_row_strx;

            if (rstrx >= 0 && rstrx < nrows_strx && cstrx >= 0 &&
                cstrx < ncols_strx) {
                // index into !!!ORIGINAL USER ROI GRID!!! (use local
                // coordinates)
                const int user_pixel =
                    (y - roi_user_local.ymin) * roi_user_local.width() +
                    (x - roi_user_local.xmin);

                map(rstrx, cstrx) = user_pixel;
            }
        }
    }

    return {multiplicity, pitch, eff, map};
};

std::vector<defs::StrixelGroupToPixelMap>
strixel_to_pixel_maps(defs::SensorConfig const &sensor_config,
                      defs::SensorPlacement const &placement,
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

defs::StrixelGroupToPixelMap
combine_maps(std::vector<defs::StrixelGroupToPixelMap> const &maps,
             std::vector<int> const &gaps) {

    if (maps.size() != gaps.size()) {
        throw std::logic_error("Gaps provided are inclompatible with "
                               "number of maps to combine. Number of gaps must "
                               "be equal the number of maps.");
    }

    auto [_, global_cols] = maps[0].map.shape();
    int global_rows = 0;
    int const m = maps[0].multiplicity;
    double const p = maps[0].pitch_um;
    auto placement_on_sensor = maps[0].placement_on_sensor;
    std::vector<int> offsets(maps.size());
    for (size_t i = 0; i < maps.size(); ++i) {

        offsets[i] = global_rows;

        if (maps[i].multiplicity != m) {
            throw std::logic_error("Maps contain incompatible multiplicities.");
        }

        if (maps[i].pitch_um != p) {
            throw std::logic_error("Maps contain incompatible pitches.");
        }

        auto [temp_rows, temp_cols] = maps[i].map.shape();
        global_cols = std::max(global_cols, temp_cols);
        global_rows = global_rows + temp_rows + gaps[i];
    }

    NDArray<ssize_t, 2> map({global_rows, global_cols}, -1);

    for (size_t i = 0; i < maps.size(); ++i) {
        auto [rows, cols] = maps[i].map.shape();

        // DEBUG
        std::cout << "DEBUG: Row offset: i = " << i
                  << ", offset: " << offsets[i] << '\n';

        for (ssize_t r = 0; r < rows; ++r) {
            for (ssize_t c = 0; c < cols; ++c) {
                map(offsets[i] + r, c) = maps[i].map(r, c);
            }
        }
    }

    // For combined maps, placement_on_sensor in principle is no longer correct
    // TODO: Decide how to handle this!
    return {m, p, placement_on_sensor, map};
}

} // namespace aare::remap::algo