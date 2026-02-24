#include "aare/Remap.hpp"

#include <algorithm>
#include <sstream>

/******************************
 * ****************************
 *     aare::remap::model
 *
 * Basic depiction of remapping
 * ****************************
 ******************************/
namespace aare::remap::model {

// Factory function (public API)
StrixelSensorConfig makeSensorConfig(SensorKey key,
                                     std::optional<Rotation> user_rot,
                                     std::optional<int> chip_id,
                                     BondShift bond_shift) {

    auto const &G = resolve::groupDescriptor(key);
    auto const geometry = resolve::chipGeometry(key);
    aare::InclusiveROI roi_module = resolve::moduleROI(key, chip_id);

    Rotation rot = Rotation::Normal;
    if (user_rot.has_value()) {
        rot = user_rot.value();
    } else if (chip_id.has_value()) {
        rot = geom::autoRotate(chip_id.value());
    }

    StrixelSensorConfig cfg(key, geometry.cols, geometry.rows,
                            geometry.guardring, bond_shift, G.multiplicator,
                            G.x_shift, G.pitch_um, G.ncols_remap, G.nrows_remap,
                            G.strixel_roi, roi_module, rot, chip_id);

    // Apply physical transforms
    if (cfg.bond_shift.x != 0 || cfg.bond_shift.y != 0)
        cfg.roi_group = aare::inclusiveroi::geom::translate(
            cfg.roi_group, cfg.bond_shift.x, cfg.bond_shift.y);

    if (cfg.rotation == Rotation::Inverse)
        cfg.roi_group = aare::inclusiveroi::geom::mirrorXY(cfg.roi_group,
                                                           cfg.cols, cfg.rows);

    return cfg;
}

} // namespace aare::remap::model

/******************************
 * ****************************
 *     aare::remap::format
 *
 * Format helpers
 * ****************************
 ******************************/
namespace aare::remap::format {

static inline std::string toString(SensorTech tech) {
    switch (tech) {
    case SensorTech::iLGAD:
        return "Technology: iLGAD";
    case SensorTech::TEW:
        return "Technology: TEW";
    default:
        return "SensorTech::Unknown";
    }
}

static inline std::string toString(SensorRevision rev) {
    switch (rev) {
    case SensorRevision::RevA:
        return "Revision: RevA";
    case SensorRevision::RevB:
        return "Revision: RevB";
    case SensorRevision::RevC:
        return "Revision: RevC";
    default:
        return "SensorRevision::Unknown";
    }
}

static inline std::string toString(SensorLayout l) {
    switch (l) {
    case SensorLayout::SingleMP25:
        return "Layout: SingleMP25 (G1, 25 um pitch)";
    case SensorLayout::SingleMP15:
        return "Layout: SingleMP15 (G2, 15 um pitch)";
    case SensorLayout::SingleMP18:
        return "Layout: SingleMP18 (G3, 18.75 um pitch)";
    case SensorLayout::SingleMP37:
        return "Layout: SingleMP37 (G4, 37.5 um pitch)";
    case SensorLayout::Quad:
        return "Layout: Quad (25 um pitch)";
    case SensorLayout::Halfmodule:
        return "Layout: Halfmodule";
    case SensorLayout::DoubleChip:
        return "Layout: DoubleChip";
    default:
        return "SensorLayout::Unknown";
    }
}

static inline std::string toString(SensorKey key) {
    return toString(key.tech) + " | " + toString(key.layout) + " | " +
           toString(key.rev);
}

static inline std::string toString(Rotation r) {
    return (r == Rotation::Normal ? "Normal" : "Inverse");
}

static inline std::string toString(StrixelSensorConfig const &c) {
    std::ostringstream os;

    os << "StrixelSensorConfig\n"
       << "  key          : " << toString(c.key) << "\n"
       << "  rotation       : " << toString(c.rotation) << "\n";

    if (c.chip_id)
        os << "  chip_id        : " << *c.chip_id << "\n";

    os << "  pixel geometry :\n"
       << "    cols x rows  : " << c.cols << " x " << c.rows << "\n"
       << "    guardring    : " << c.guardring << "\n"
       << "    bond_shift_x : " << c.bond_shift.x << "\n"
       << "    bond_shift_y : " << c.bond_shift.y << "\n";

    os << "  strixel geometry :\n"
       << "    multiplicator : " << c.multiplicator << "\n"
       << "    shift_x       : " << c.shift_x << "\n"
       << "    pitch_um      : " << c.pitch_um << "\n"
       << "    remap cols    : " << c.cols_remap << "\n"
       << "    remap rows    : " << c.rows_remap << "\n";

    os << "  roi_group  : " << c.roi_group << "\n"
       << "  roi_module : " << c.roi_module << "\n";

    return os.str();
}

inline std::ostream &operator<<(std::ostream &os,
                                StrixelSensorConfig const &c) {
    return os << toString(c);
}

} // namespace aare::remap::format

/******************************
 * ****************************
 *      aare::remap::geom
 *
 * Geometric helpers
 * ****************************
 ******************************/
namespace aare::remap::geom {

using namespace aare::remap::model;

aare::InclusiveROI alignROIs(aare::InclusiveROI const &roi_user,
                             aare::InclusiveROI const &roi_base) {
    const int dx = roi_base.xmin;
    const int dy = roi_base.ymin; // + bond_shift_y;

    return {roi_user.xmin - dx, roi_user.xmax - dx, roi_user.ymin - dy,
            roi_user.ymax - dy};
    // return roi::geom::translate(roi_user, roi_base.xmin, roi_base.ymin);
}

Rotation autoRotate(int chip_id) {
    return (chip_id == 1   ? Rotation::Normal
            : chip_id == 6 ? Rotation::Inverse
                           : throw std::runtime_error("Unknown chip_id"));
}

} // namespace aare::remap::geom

/******************************
 * ****************************
 *      aare::remap::algo
 *
 * Remapping algorithms
 * ****************************
 ******************************/
namespace aare::remap::algo {

using namespace aare::remap::model;
using namespace aare::remap::format;

MappingResult generateUnitMap(aare::InclusiveROI const &roi_user,
                              aare::InclusiveROI const &roi_group,
                              int multiplicator, Rotation rot, int shifty) {
    // Helper to make sure that we work with a correct number of strixel columns
    // (i.e. that we do not map pixel columns if the ncols in ASIC pixel
    // coordinates is not a multiple of strixel ncols)
    if (roi_group.width() % multiplicator != 0)
        throw std::logic_error(
            "Group ROI width not divisible by multiplicator");

    const int tot_ncols_strx = roi_group.width() / multiplicator;

    // Define mod ordering (Normal or Inverse)
    std::vector<int> mods(multiplicator);
    for (int i = 0; i < multiplicator; i++)
        mods[i] = i;
    if (rot == Rotation::Inverse)
        std::reverse(mods.begin(), mods.end());

    // -- 1) Compute effective ROI = intersection( roi_user, roi_group )
    aare::InclusiveROI eff =
        aare::inclusiveroi::geom::intersect(roi_user, roi_group);
    if (eff.xmax < eff.xmin || eff.ymax < eff.ymin) {
        return {{}, 0, 0, -1, aare::InclusiveROI::emptyROI()}; // empty
    }

    // DEBUG
    std::cout << "Result of intersecting ROIs " << eff << '\n';

    //-- 2) Determine min/max row/col of strixel grid before allocating
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

            const int m = dx % multiplicator;
            const int col_strx = dx / multiplicator;
            const int row_strx = dy * multiplicator + mods[m] + shifty;

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
        return {{}, 0, 0, multiplicator, eff};
    }

    const int nrows_strx = max_row_strx - min_row_strx + 1;
    const int ncols_strx = max_col_strx - min_col_strx + 1;

    // Allocate strixel grid order map
    aare::NDArray<ssize_t, 2> ord({nrows_strx, ncols_strx}, -1);

    // -- 3) For each ASIC pixel in eff ROI, compute remapped (row,col) in group
    //       local coordinates
    for (int y = eff.ymin; y <= eff.ymax; ++y) {
        for (int x = eff.xmin; x <= eff.xmax; ++x) {

            const int dx = x - roi_group.xmin;
            const int dy = (y - roi_group.ymin);

            const int m = dx % multiplicator; // since eff is intersected with
                                              // roi_group, dx >= 0, so no issue
            const int col_strx = dx / multiplicator;
            const int row_strx = dy * multiplicator + mods[m] + shifty;

            if (col_strx < min_col_strx || row_strx < min_row_strx)
                continue;

            const int cstrx = col_strx - min_col_strx;
            const int rstrx = row_strx - min_row_strx;

            if (rstrx >= 0 && rstrx < nrows_strx && cstrx >= 0 &&
                cstrx < ncols_strx) {
                // index into ORIGINAL USER ROI GRID
                const int user_pixel = (y - roi_user.ymin) * roi_user.width() +
                                       (x - roi_user.xmin);

                ord(rstrx, cstrx) = user_pixel;
            }
        }
    }

    return {ord, nrows_strx, ncols_strx, multiplicator, eff};
}

MappingResult joinQuadMaps(MappingResult const &bottom,
                           MappingResult const &top, int gap_rows) {
    if (bottom.cols == 0 && top.cols == 0)
        return {{}, 0, 0, -1, aare::InclusiveROI::emptyROI()};

    if (bottom.multiplicator != top.multiplicator) {
        throw std::runtime_error("Multiplicators not compatible.");
    }

    const int global_cols = std::max(bottom.cols, top.cols);
    const int global_rows = bottom.rows + gap_rows + top.rows;

    aare::NDArray<ssize_t, 2> ord({global_rows, global_cols}, -1);

    // --- copy bottom half ---
    for (int r = 0; r < bottom.rows; ++r) {
        for (int c = 0; c < bottom.cols; ++c) {
            ord(r, c) = bottom.order_map(r, c);
        }
    }

    // --- copy top half ---
    const int top_row_offset = bottom.rows + gap_rows;
    for (int r = 0; r < top.rows; ++r) {
        for (int c = 0; c < top.cols; ++c) {
            ord(top_row_offset + r, c) = top.order_map(r, c);
        }
    }

    // --- smallest common denominator ROI (pixel space)
    aare::InclusiveROI scd = aare::inclusiveroi::geom::intersect(
        bottom.scd_roi_pixel, top.scd_roi_pixel);

    return {ord, global_cols, global_rows, bottom.multiplicator, scd};
}

MappingResult generateMPStrixelMapping(
    aare::InclusiveROI const &roi_user_module, SensorKey key, int chip_id,
    std::optional<Rotation> user_rot, BondShift bond_shift) {
    // -- 1) initialize config
    auto config = makeSensorConfig(key, user_rot, chip_id, bond_shift);
    // static_assert(std::is_same_v<decltype(config.pitch_um), double>);
    std::cout << "Initialized config: " << config << std::endl;

    if (!(key.layout == SensorLayout::SingleMP25 ||
          key.layout == SensorLayout::SingleMP15 ||
          key.layout == SensorLayout::SingleMP18)) {
        throw std::runtime_error("Invalid sensor type!");
    } /* else {
      std::cout << "Sensor type " << config.label << std::endl;
    } */

    // -- 2) transform user ROI to sensor-local coordinates
    const aare::InclusiveROI roi_user_local =
        aare::remap::geom::alignROIs(roi_user_module, config.roi_module);
    std::cout << "Transformed user ROI: " << roi_user_local << std::endl;

    // -- 3) remap
    auto m = generateUnitMap(roi_user_local, config.roi_group,
                             config.multiplicator, config.rotation);
    if (m.cols > 0)
        return m;

    // No valid region → return empty
    return {{}, 0, 0, config.multiplicator, aare::InclusiveROI::emptyROI()};
}

MappingResult
generateQuadStrixelMapping(aare::InclusiveROI const &roi_user_module,
                           SensorKey key, std::optional<Rotation> user_rot,
                           BondShift bond_shift) {
    // -- 1) initialize configs
    auto config = makeSensorConfig(key, user_rot, std::nullopt, bond_shift);

    if (!(key.layout == SensorLayout::Quad)) {
        throw std::runtime_error("Invalid sensor type!");
    }

    // -- 2) transform user module coordinates to local coordinates
    aare::InclusiveROI roi_user_local =
        aare::remap::geom::alignROIs(roi_user_module, config.roi_module);
    std::cout << "Transformed user ROI: " << roi_user_local << std::endl;

    // -- 3) get definition of half quad ROI
    const aare::InclusiveROI halfquad = config.roi_group;

    // -- 4) remap bottom half (normal mod order)
    auto bottom =
        generateUnitMap(roi_user_local, halfquad, config.multiplicator,
                        Rotation::Normal, /*shifty=*/0);

    // -- 5) top half (mirrored ROI, inverse mod order)
    aare::InclusiveROI top_halfquad =
        aare::inclusiveroi::geom::mirrorXY(halfquad, config.cols, config.rows);
    auto top =
        generateUnitMap(roi_user_local, top_halfquad, config.multiplicator,
                        Rotation::Inverse, /*shifty=*/0);

    // -- 6) compose into quad
    constexpr int gap_rows = 12; // I don't like that this is hardcoded here
    return joinQuadMaps(bottom, top, gap_rows);
}

} // namespace aare::remap::algo

/******************************
 * ****************************
 *      remap::resolve
 *
 * Resolvers that load from the
 * right config. Only here we
 * have a config connection!
 * ****************************
 ******************************/
namespace aare::remap::resolve {
using namespace aare::remap::config;

GroupDescriptor const &groupDescriptor(SensorKey key) {

    switch (key.tech) {

    // ================= iLGAD =================
    case SensorTech::iLGAD:
        switch (key.layout) {
        case SensorLayout::SingleMP25:
            return SingleChipMP_iLGAD::P25;
        case SensorLayout::SingleMP15:
            return SingleChipMP_iLGAD::P15;
        case SensorLayout::SingleMP18:
            return SingleChipMP_iLGAD::P18;
        case SensorLayout::Quad:
            return Quad_iLGAD::Half;
        default:
            throw std::runtime_error("Unsupported SensorLayout for iLGAD");
        }

    // ================= TEW ===================
    case SensorTech::TEW:
        switch (key.layout) {
        case SensorLayout::SingleMP25:
            return SingleChipMP_TEW::P25;
        case SensorLayout::SingleMP15:
            return SingleChipMP_TEW::P15;
        case SensorLayout::SingleMP18:
            return SingleChipMP_TEW::P18;
        default:
            throw std::runtime_error("Unsupported SensorLayout for TEW");
        }

    default:
        throw std::runtime_error("Unsupported SensorTech");
    }
}

ChipGeometry chipGeometry(SensorKey key) {

    using SL = SensorLayout;
    using ST = SensorTech;

    switch (key.layout) {
    case SL::SingleMP25:
    case SL::SingleMP15:
    case SL::SingleMP18:
        switch (key.tech) {
        case ST::iLGAD:
            return SingleChipMP_iLGAD::geometry;
        case ST::TEW:
            return SingleChipMP_TEW::geometry;
        default:
            throw std::logic_error("Unsupported SensorTech");
        }
    case SL::Quad:
        switch (key.tech) {
        case ST::iLGAD:
            return Quad_iLGAD::geometry;
        default:
            throw std::logic_error(
                "Unsupported SensorTech for SensorLayout Quad");
        }

    default:
        throw std::logic_error("Unsupported SensorLayout");
    }
}

aare::InclusiveROI moduleROI(SensorKey key, std::optional<int> chip_id) {

    using SL = SensorLayout;
    using ST = SensorTech;

    auto requireChip = [&](bool needed) {
        if (needed && !chip_id)
            throw std::logic_error("chip_id required for this layout");
        if (!needed && chip_id)
            throw std::logic_error("chip_id must not be set for this layout");
    };

    switch (key.layout) {

    // ---------- Single-chip multipitch ----------
    case SL::SingleMP25:
    case SL::SingleMP15:
    case SL::SingleMP18: {
        requireChip(true);

        const int cid = *chip_id;
        if (cid != 1 && cid != 6)
            throw std::out_of_range("Unsupported chip_id (expected 1 or 6)");

        switch (key.tech) {
        case ST::iLGAD:
            return (cid == 1) ? SingleChipMP_iLGAD::chip1
                              : SingleChipMP_iLGAD::chip6;
        case ST::TEW:
            return (cid == 1) ? SingleChipMP_TEW::chip1
                              : SingleChipMP_TEW::chip6;
        default:
            throw std::logic_error("Unsupported SensorTech");
        }
    }

    // ---------- Quad layout ----------
    case SL::Quad: {
        requireChip(false);

        switch (key.tech) {
        case ST::iLGAD:
            return Quad_iLGAD::coords;
        default:
            throw std::logic_error("Quad layout not supported for this tech");
        }
    }

    default:
        throw std::logic_error("Unsupported SensorLayout");
    }
}

} // namespace aare::remap::resolve