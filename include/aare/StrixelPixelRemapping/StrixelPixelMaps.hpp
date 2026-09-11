#include "aare/StrixelPixelRemapping/BaseStrixelPixelMap.hpp"
#include "aare/StrixelPixelRemapping/StrixelPixelRemapConfig.hpp"

namespace aare::remap {

namespace detail {
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

    return {std::move(combined), effective_roi};
}

} // namespace detail

/**
 * @brief StrixeltoPixelMap for a JUNGFRAU single-chip multi-pitch iLGAD sensor
 */
class Jungfrau_iLGAD_StrixelPixelMap : public aare::remap::StrixelPixelMap<3> {

  public:
    /**
     * @brief Construct a new Jungfrau_iLGAD_StrixelPixelMap object.
     * @param module_placement Placement and orientation of the sensor on the
     * module.
     * @param bond_shift Bonding shift applied before the configured sensor
     * rotation.
     */
    Jungfrau_iLGAD_StrixelPixelMap(
        const defs::SensorModulePlacement &module_placement,
        const defs::BondShift &bond_shift = {0, 0})
        : StrixelPixelMap<3>(config::jungfrau::SingleChipMP_iLGAD,
                             module_placement, bond_shift) {}
};

/**
 * @brief StrixeltoPixelMap for a JUNGFRAU single-chip multi-pitch TEW sensor
 */
class Jungfrau_TEW_StrixelPixelMap : public aare::remap::StrixelPixelMap<3> {

  public:
    /**
     * @brief Construct a new Jungfrau_TEW_StrixelPixelMap object.
     * @param module_placement Placement and orientation of the sensor on the
     * module.
     * @param bond_shift Bonding shift applied before the configured sensor
     * rotation.
     */
    Jungfrau_TEW_StrixelPixelMap(
        const defs::SensorModulePlacement &module_placement,
        const defs::BondShift &bond_shift = {0, 0})
        : StrixelPixelMap<3>(config::jungfrau::SingleChipMP_TEW,
                             module_placement, bond_shift) {}
};

/**
 * @brief StrixeltoPixelMap for a JUNGFRAU single-chip quad iLGAD sensor
 * @note the individual strixel groups are combined into a single map with a gap
 * of invalid strixel rows in between.
 */
class Jungfrau_iLGAD_Quad_StrixelPixelMap
    : public aare::remap::StrixelPixelMap<2, 1> {

  public:
    /**
     * @brief Construct a new Jungfrau_iLGAD_Quad_StrixelPixelMap object.
     * @param bond_shift Bonding shift applied before the configured sensor
     * rotation.
     */
    Jungfrau_iLGAD_Quad_StrixelPixelMap(const defs::BondShift &bond_shift = {0,
                                                                             0})
        : StrixelPixelMap<2, 1>(config::jungfrau::Quad_iLGAD,
                                aare::remap::config::jungfrau::Quad,
                                bond_shift) {}

    /**
     * @brief Calculate the strixel-to-pixel order maps for all strixel groups
     * based on the user-specified ROI.
     * @param user_roi User-specified ROI in the module's native coordinate
     * system.
     * @note This implementation combines the two strixel groups into a single
     * map with a gap of invalid strixel rows in between.
     */
    void calculate_map(const ROI &user_roi) override {
        m_user_roi = toInclusiveROI(user_roi);

        auto quad_bottom_half =
            strixel_to_pixel_map(m_sensorconfig.group_configs[0]);
        auto quad_top_half =
            strixel_to_pixel_map(m_sensorconfig.group_configs[1]);

        // TODO: good idea to combine?
        m_group_maps[0] = detail::combine_group_maps(
            quad_bottom_half, quad_top_half,
            config::jungfrau::Quad_iLGAD_strixel_gap_rows);
    }
};

} // namespace aare::remap
