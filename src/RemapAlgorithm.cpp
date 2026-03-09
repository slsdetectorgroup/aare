#include "aare/RemapAlgorithm.hpp"

#include <algorithm>

namespace aare::remap::algo {

// Is it better to pass defs::SensorGroupConfig const& and return a copy?
void apply_rotation_shift(defs::SensorGroupConfig &cfg,
                          defs::BondShift bond_shift, defs::Rotation rot) {
    // Apply physical transforms
    if (bond_shift.x != 0 || bond_shift.y != 0)
        cfg.placement_on_sensor = aare::inclusiveroi::geom::translate(
            cfg.placement_on_sensor, bond_shift.x, bond_shift.y);

    if (rot == defs::Rotation::Inverse)
        cfg.placement_on_sensor = aare::inclusiveroi::geom::mirrorXY(
            cfg.placement_on_sensor, cfg.pixel.num_pix_x, cfg.pixel.num_pix_y);
}

defs::StrixelGroupToPixelMap
strixel_to_pixel_map(defs::SensorGroupConfig group_config,
                     defs::SensorPlacement placement, InclusiveROI roi_user,
                     defs::BondShift bond_shift) {

    int multiplicity = group_config.strixel.multiplicity;
    double pitch = group_config.strixel.pitch_um;
    defs::Rotation rot = placement.rotation;

    // Helper to make sure that we work with a correct number of strixel columns
    // (i.e. that we do not map pixel columns if the ncols in ASIC pixel
    // coordinates is not a multiple of strixel ncols)
    if (group_config.placement_on_sensor.width() % multiplicity != 0)
        throw std::logic_error("Group ROI width not divisible by multiplicity");

    const int tot_ncols_strx =
        group_config.placement_on_sensor.width() / multiplicity;

    // Define mod ordering (Normal or Inverse)
    std::vector<int> mods(multiplicity);
    for (int i = 0; i < multiplicity; i++)
        mods[i] = i;
    if (rot == defs::Rotation::Inverse)
        std::reverse(mods.begin(), mods.end());

    // -- 1) Transform user roi (rx_roi) into sensor-local coordinates
    InclusiveROI roi_user_local =
        inclusiveroi::geom::alignROIs(roi_user, placement.placement_on_module);
    std::cout << "Transformed user ROI: " << roi_user_local << std::endl;

    // DEBUG
    std::cout << "DEBUG: Group ROI before transformation "
              << group_config.placement_on_sensor << '\n';

    // -- 2) Apply transforms (if necessary)
    // -- 2a) bond_shift
    // -- 2b) rotation
    // -- IMPORTANT: bond_shift BEFORE rotation!
    apply_rotation_shift(group_config, bond_shift, placement.rotation);

    // -- 2c) AFTER applying the transformations, we can grab the correct
    // strixel roi
    auto roi_group = group_config.placement_on_sensor;

    // DEBUG
    std::cout << "DEBUG: Group ROI after transformation "
              << group_config.placement_on_sensor << '\n';

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
        // nothing mapped
        return {multiplicity, pitch, eff, {}};
    }

    const int nrows_strx = max_row_strx - min_row_strx + 1;
    const int ncols_strx = max_col_strx - min_col_strx + 1;

    // Allocate strixel grid order map
    aare::NDArray<ssize_t, 2> map({nrows_strx, ncols_strx}, -1);

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
strixel_to_pixel_maps(defs::SensorConfig sensor_config,
                      defs::SensorPlacement placement, InclusiveROI roi_user,
                      defs::BondShift bond_shift) {
    std::vector<defs::StrixelGroupToPixelMap> maps;
    for (auto &group_config : sensor_config.group_configs) {
        maps.emplace_back(strixel_to_pixel_map(group_config, placement,
                                               roi_user, bond_shift));
    }
    return maps;
}

} // namespace aare::remap::algo