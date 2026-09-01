#pragma once

#include "aare/StrixelPixelRemapAlgorithm.hpp"
#include "aare/StrixelPixelRemapConfig.hpp"

namespace aare::remap::generate {

// ============================================================
// Internal implementation details
// ============================================================
// Current quick-and-dirty inclusion of a helper function to combine group maps.
// This becomes necessary for sensors like Quad_iLGAD that combine two
// independent mapping regions separated by invalid gap rows. (Used to live as
// `combine_maps` in RemapAlgorithm, but arguably should not be part of that
// public API). Eventually, one could instead split RemapGenerate into hpp and
// cpp and make this one live in an unnamed namespace inside the cpp so that it
// cannot be exposed to the user through the public API (if we want to avoid
// that)
namespace detail {
/**
 * @brief Combine TWO vertically ordered group remapping maps.
 *
 * Concatenates the maps in the order provided, inserting `gap_rows`
 * rows of invalid entries (-1) between consecutive groups.
 *
 * The input maps must be ordered in the desired output order.
 * Both maps must have the same number of columns.
 *
 * The returned effective ROI is the bounding ROI covering the
 * effective ROIs of both input groups. It describes the physical
 * source-pixel region and does not encode the artificial strixel
 * gap rows in the output map.
 *
 * @param first Group map that comes first in output space.
 * @param second Group map that comes second in output space.
 * @param gap_rows Number of invalid strixel rows inserted between groups.
 *
 * @return Combined strixel-to-pixel map.
 *
 * @throws std::logic_error if group maps have different widths.
 */
defs::StrixelGroupToPixelMap
combine_group_maps(defs::StrixelGroupToPixelMap const &first,
                   defs::StrixelGroupToPixelMap const &second,
                   size_t gap_rows) {

    const ssize_t ncols = first.map.shape(1);

    // Make sure both maps have the same width.
    if (second.map.shape(1) != ncols) {
        throw std::logic_error("Cannot combine maps with different numbers "
                               "of columns");
    }

    // Check effective ROIs line up
    if (first.effective_roi.xmin != second.effective_roi.xmin ||
        first.effective_roi.xmax != second.effective_roi.xmax) {
        throw std::logic_error(
            "Cannot combine group maps with different x extents");
    }

    // Calculate total number of output rows.
    ssize_t total_rows = 0;
    total_rows = first.map.shape(0) + second.map.shape(0) +
                 static_cast<ssize_t>(gap_rows);

    // Allocate and initialize with -1.
    //
    // -1 represents an output strixel position that has
    // no corresponding input pixel.
    NDArray<ssize_t, 2> combined({total_rows, ncols}, -1);

    // Copy maps into the combined output.
    auto copy_map = [&](auto const &source, ssize_t destination_row) {
        const ssize_t nrows = source.map.shape(0);

        for (ssize_t row = 0; row < nrows; ++row) {
            for (ssize_t col = 0; col < ncols; ++col) {
                combined(destination_row + row, col) = source.map(row, col);
            }
        }
    };

    const ssize_t first_row = 0;
    const ssize_t second_row = first.map.shape(0) + gap_rows;

    copy_map(first, first_row);
    copy_map(second, second_row);

    // Bounding pixel ROI covered by the combined groups.
    InclusiveROI effective_roi = first.effective_roi;

    effective_roi.xmin =
        std::min(effective_roi.xmin, second.effective_roi.xmin);
    effective_roi.xmax =
        std::max(effective_roi.xmax, second.effective_roi.xmax);
    effective_roi.ymin =
        std::min(effective_roi.ymin, second.effective_roi.ymin);
    effective_roi.ymax =
        std::max(effective_roi.ymax, second.effective_roi.ymax);

    return {.map = std::move(combined), .effective_roi = effective_roi};
}
} // namespace detail

// ============================================================
// Public generators
// ============================================================

/************************************
 * Single chip, multi-pitch, iLGAD
 *
 * Individual groups:
 *  - jungfrau_ilgad_singlechip_25um_strixel_map
 *  - jungfrau_ilgad_singlechip_15um_strixel_map
 *  - jungfrau_ilgad_singlechip_18um_strixel_map
 *
 * All in one vector:
 *  - jungfrau_ilgad_singlechip_multipitch_strixel_maps
 ************************************/

/**
 * @brief Generate a strixel-to-pixel remapping map for the 25 um strixel pitch
 * region on a JUNGFRAU single-chip multi-pitch iLGAD sensor.
 *
 * The returned map converts pixels from the user-specified ASIC readout ROI
 * into the corresponding strixel coordinate system.
 *
 * @param rx_roi
 *      ROI in the user/input ASIC coordinate system.
 *
 * @param placement
 *      Placement and orientation of the sensor on the module.
 *
 * @param bs
 *      Bonding shift applied before the configured sensor placement rotation.
 *
 * @return
 *      A strixel-to-pixel remapping map with flattened indices
 *      into the user-provided ROI.
 */
inline defs::StrixelGroupToPixelMap jungfrau_ilgad_singlechip_25um_strixel_map(
    InclusiveROI rx_roi, defs::SensorModulePlacement placement,
    defs::BondShift bs = {0, 0}) {
    return algo::strixel_to_pixel_map(config::jungfrau::SingleChipMP_iLGAD_P25,
                                      config::jungfrau::SingleChipMP_iLGAD_pix,
                                      placement, rx_roi, bs);
}

/**
 * @brief Generate a strixel-to-pixel remapping map for the 15 um strixel pitch
 * region on a JUNGFRAU single-chip multi-pitch iLGAD sensor.
 *
 * The returned map converts pixels from the user-specified ASIC readout ROI
 * into the corresponding strixel coordinate system.
 *
 * @param rx_roi
 *      ROI in the user/input ASIC coordinate system.
 *
 * @param placement
 *      Placement and orientation of the sensor on the module.
 *
 * @param bs
 *      Bonding shift applied before the configured sensor placement rotation.
 *
 * @return
 *      A strixel-to-pixel remapping map with flattened indices
 *      into the user-provided ROI.
 */
inline defs::StrixelGroupToPixelMap jungfrau_ilgad_singlechip_15um_strixel_map(
    InclusiveROI rx_roi, defs::SensorModulePlacement placement,
    defs::BondShift bs = {0, 0}) {
    return algo::strixel_to_pixel_map(config::jungfrau::SingleChipMP_iLGAD_P15,
                                      config::jungfrau::SingleChipMP_iLGAD_pix,
                                      placement, rx_roi, bs);
};

/**
 * @brief Generate a strixel-to-pixel remapping map for the 18.75 um strixel
 * pitch region on a JUNGFRAU single-chip multi-pitch iLGAD sensor.
 *
 * The returned map converts pixels from the user-specified ASIC readout ROI
 * into the corresponding strixel coordinate system.
 *
 * @param rx_roi
 *      ROI in the user/input ASIC coordinate system.
 *
 * @param placement
 *      Placement and orientation of the sensor on the module.
 *
 * @param bs
 *      Bonding shift applied before the configured sensor placement rotation.
 *
 * @return
 *      A strixel-to-pixel remapping map with flattened indices
 *      into the user-provided ROI.
 */
inline defs::StrixelGroupToPixelMap jungfrau_ilgad_singlechip_18um_strixel_map(
    InclusiveROI rx_roi, defs::SensorModulePlacement placement,
    defs::BondShift bs = {0, 0}) {
    return algo::strixel_to_pixel_map(config::jungfrau::SingleChipMP_iLGAD_P18,
                                      config::jungfrau::SingleChipMP_iLGAD_pix,
                                      placement, rx_roi, bs);
};

/**
 * @brief Generate all strixel-to-pixel maps for a JUNGFRAU single-chip
 *        multi-pitch iLGAD sensor using an explicit sensor placement.
 *
 * This overload allows the caller to specify an arbitrary sensor placement
 * instead of selecting one of the predefined chip placements.
 *
 * @param rx_roi
 *      ROI in the user/input ASIC coordinate system.
 *
 * @param placement
 *      Placement and orientation of the sensor on the module.
 *
 * @param bs
 *      Bonding shift applied before the configured sensor rotation.
 *
 * @return
 *      One remapping map per configured strixel group.
 */
inline auto jungfrau_ilgad_singlechip_multipitch_strixel_maps(
    InclusiveROI rx_roi, defs::SensorModulePlacement placement,
    defs::BondShift bs = {0, 0}) {

    return algo::strixel_to_pixel_maps(config::jungfrau::SingleChipMP_iLGAD,
                                       placement, rx_roi, bs);
};

/************************************
 * Single chip, multi-pitch, TEW
 *
 *  * Individual groups:
 *  - jungfrau_tew_singlechip_25um_strixel_map
 *  - jungfrau_tew_singlechip_15um_strixel_map
 *  - jungfrau_tew_singlechip_18um_strixel_map
 *
 * All in one vector:
 *  - jungfrau_tew_singlechip_multipitch_strixel_maps
 ************************************/

/**
 * @brief Generate a strixel-to-pixel map for the 25 um strixel pitch
 * region on a JUNGFRAU single-chip multi-pitch TEW sensor.
 *
 * @param rx_roi
 *      ROI in the user/input ASIC coordinate system.
 * @param placement
 *      Placement and orientation of the sensor on the module.
 * @param bs
 *      Bonding shift applied before the configured sensor rotation.
 *
 * @return
 *      Strixel-to-pixel remapping map with indices into @p rx_roi.
 */
inline defs::StrixelGroupToPixelMap
jungfrau_tew_singlechip_25um_strixel_map(InclusiveROI rx_roi,
                                         defs::SensorModulePlacement placement,
                                         defs::BondShift bs = {0, 0}) {
    return algo::strixel_to_pixel_map(config::jungfrau::SingleChipMP_TEW_P25,
                                      config::jungfrau::SingleChipMP_TEW_pix,
                                      placement, rx_roi, bs);
}

/**
 * @brief Generate a strixel-to-pixel map for the 15 um strixel pitch
 * region on a JUNGFRAU single-chip multi-pitch TEW sensor.
 */
inline defs::StrixelGroupToPixelMap
jungfrau_tew_singlechip_15um_strixel_map(InclusiveROI rx_roi,
                                         defs::SensorModulePlacement placement,
                                         defs::BondShift bs = {0, 0}) {
    return algo::strixel_to_pixel_map(config::jungfrau::SingleChipMP_TEW_P15,
                                      config::jungfrau::SingleChipMP_TEW_pix,
                                      placement, rx_roi, bs);
};

/**
 * @brief Generate a strixel-to-pixel map for the 18.75 um strixel pitch
 * region on a JUNGFRAU single-chip multi-pitch TEW sensor.
 */
inline defs::StrixelGroupToPixelMap
jungfrau_tew_singlechip_18um_strixel_map(InclusiveROI rx_roi,
                                         defs::SensorModulePlacement placement,
                                         defs::BondShift bs = {0, 0}) {
    return algo::strixel_to_pixel_map(config::jungfrau::SingleChipMP_TEW_P18,
                                      config::jungfrau::SingleChipMP_TEW_pix,
                                      placement, rx_roi, bs);
};

/**
 * @brief Generate all strixel-to-pixel maps for a JUNGFRAU single-chip
 *        multi-pitch TEW sensor using an explicit sensor placement.
 *
 * @param rx_roi
 *      ROI in the user/input ASIC coordinate system.
 *
 * @param placement
 *      Placement and orientation of the sensor on the module.
 *
 * @param bs
 *      Bonding shift applied before the configured sensor rotation.
 *
 * @return
 *      One remapping map per configured strixel group.
 */
inline auto jungfrau_tew_singlechip_multipitch_strixel_maps(
    InclusiveROI rx_roi, defs::SensorModulePlacement placement,
    defs::BondShift bs = {0, 0}) {

    return algo::strixel_to_pixel_maps(config::jungfrau::SingleChipMP_TEW,
                                       placement, rx_roi, bs);
};

/************************************
 * Quad, 25 um, iLGAD
 *
 * Individual halves:
 *  - jungfrau_ilgad_quadbottom_25um_strixel_map
 *  - jungfrau_ilgad_quadtop_25um_strixel_map
 *
 * Vector with both halves:
 *  - jungfrau_ilgad_quad_25um_strixel_maps
 *
 * Complete, combined map of full sensor:
 *  - jungfrau_ilgad_quad_25um_strixel_map
 *
 * NOTE: In principle, we could hide or remove the individual halves and hide
 * the vector version, only exposing the complete sensor map generator.
 ************************************/

/**
 * @brief Generate the remapping map for the bottom half of a JUNGFRAU
 *        25 um iLGAD quad sensor.
 *
 * The returned map represents only the bottom strixel group of the quad
 * sensor. It does not include the top half or the central strixel gap.
 *
 * @param rx_roi
 *      ROI in the user/input ASIC coordinate system.
 *
 * @param placement
 *      Placement and orientation of the quad sensor on the module.
 *
 * @return
 *      Strixel-to-pixel remapping map for the bottom sensor half.
 */
inline defs::StrixelGroupToPixelMap jungfrau_ilgad_quadbottom_25um_strixel_map(
    InclusiveROI rx_roi, defs::SensorModulePlacement placement,
    defs::BondShift bs = {0, 0}) {
    return algo::strixel_to_pixel_map(config::jungfrau::Quad_iLGAD_bottomhalf,
                                      config::jungfrau::Quad_iLGAD_pix,
                                      placement, rx_roi, bs);
}

/**
 * @brief Generate the remapping map for the top half of a JUNGFRAU
 *        25 um iLGAD quad sensor.
 *
 * The returned map represents only the top strixel group of the quad
 * sensor. It does not include the bottom half or the central strixel gap.
 *
 * @param rx_roi
 *      ROI in the user/input ASIC coordinate system.
 *
 * @param placement
 *      Placement and orientation of the quad sensor on the module.
 *
 * @return
 *      Strixel-to-pixel remapping map for the top sensor half.
 */
inline defs::StrixelGroupToPixelMap
jungfrau_ilgad_quadtop_25um_strixel_map(InclusiveROI rx_roi,
                                        defs::SensorModulePlacement placement,
                                        defs::BondShift bs = {0, 0}) {
    return algo::strixel_to_pixel_map(config::jungfrau::Quad_iLGAD_tophalf,
                                      config::jungfrau::Quad_iLGAD_pix,
                                      placement, rx_roi, bs);
}

/**
 * @brief Generate the individual strixel-to-pixel maps for a JUNGFRAU
 *        25 um iLGAD quad sensor.
 *
 * The returned vector contains one map for each of the two sensor halves:
 *
 *   - bottom half
 *   - top half
 *
 * The maps remain separate and do not contain the central strixel gap.
 * Use jungfrau_ilgad_quad_25um_strixel_map() to obtain a single combined
 * map including the configured gap.
 *
 * @param rx_roi
 *      ROI in the user/input ASIC coordinate system.
 *
 * @param placement
 *      Placement and orientation of the quad sensor on the module.
 *
 * @return
 *      Two strixel-group remapping maps, ordered according to the quad
 *      sensor configuration.
 */
inline auto
jungfrau_ilgad_quad_25um_strixel_maps(InclusiveROI rx_roi,
                                      defs::SensorModulePlacement placement,
                                      defs::BondShift bs = {0, 0}) {

    return algo::strixel_to_pixel_maps(config::jungfrau::Quad_iLGAD, placement,
                                       rx_roi, bs);
}

/**
 * @brief Generate the combined strixel-to-pixel map for a JUNGFRAU
 *        25 um iLGAD quad sensor.
 *
 * The two sensor halves with separate mapping rules are combined into one
 * strixel coordinate system. The configured central gap between the two halves
 * is represented by invalid entries in the corresponding gap rows of the
 * returned map.
 *
 * @param rx_roi
 *      ROI in the user/input ASIC coordinate system.
 *
 * @param placement
 *      Placement and orientation of the quad sensor on the module.
 *
 * @param bs
 *      Bonding shift applied before the configured sensor rotation.
 *
 * @return
 *      A single combined strixel-to-pixel remapping map representing
 *      both sensor halves and the central strixel gap.
 */
inline defs::StrixelGroupToPixelMap
jungfrau_ilgad_quad_25um_strixel_map(InclusiveROI rx_roi,
                                     defs::SensorModulePlacement placement,
                                     defs::BondShift bs = {0, 0}) {
    auto maps = jungfrau_ilgad_quad_25um_strixel_maps(rx_roi, placement, bs);
    return detail::combine_group_maps(
        maps[0], maps[1], config::jungfrau::Quad_iLGAD_strixel_gap_rows);
}
} // namespace aare::remap::generate
