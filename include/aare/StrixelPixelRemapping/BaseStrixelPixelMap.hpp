#pragma once
#include "aare/StrixelPixelRemapping/StrixelPixelRemapDefs.hpp"

namespace aare::remap {

using namespace defs;

// Internal geometry helper
namespace detail {
/**
 * @brief Apply physical transformations to a sensor-local ROI.
 *
 * IMPORTANT:
 * Bond shifts are applied before rotation.
 * The order is intentional because bond shifts are defined in the
 * sensor's native coordinate system.
 *
 * This function is not intended to be exposed to the public
 */
InclusiveROI inline update_pixel_group_placement(
    InclusiveROI roi, defs::SensorPixelGeometry const &pixel,
    defs::BondShift bond_shift, defs::Rotation rot) {
    // If there is a bond shift, translate the roi
    if (bond_shift.x != 0 || bond_shift.y != 0)
        roi = aare::inclusiveroi::geom::translate(roi, bond_shift.x,
                                                  bond_shift.y);

    // If there is a rotation given, mirror in X and Y (emulates a rotation)
    if (rot == defs::Rotation::Rotate180)
        roi = aare::inclusiveroi::geom::mirrorXY(roi, pixel.num_pix_x / 2,
                                                 pixel.num_pix_y / 2);

    return roi;
}
} // namespace detail

namespace detail {
struct StrixelPixelMapBindingAccess;
}

template <std::size_t N, std::size_t M = N> class StrixelPixelMap {
    friend struct detail::StrixelPixelMapBindingAccess;

  public:
    StrixelPixelMap(const SensorConfig<N> &sensor_config,
                    const SensorModulePlacement &module_placement,
                    const BondShift &bond_shift = BondShift{0, 0});
    virtual ~StrixelPixelMap() = default;

    /**
     * @brief Calculate the strixel-to-pixel order maps for all strixel groups
     * based on the user-specified ROI.
     * @param user_roi User-specified ROI in the module's native coordinate
     * system.
     */
    virtual void calculate_map(const ROI &user_roi);

    /**
     * @brief Apply the strixel-to-pixel remapping to an input array.
     * @param user_roi User-specified ROI in the module's native coordinate
     * system.
     * @param input Input array to be remapped.
     * @return std::array<NDArray<T, 2>, N> Remapped arrays for each strixel
     * group.
     */
    template <typename T>
    std::array<NDArray<T, 2>, M> operator()(const ROI &user_roi,
                                            const NDView<T, 2> input);

    /**
     * @brief Apply the strixel-to-pixel remapping to an input array.
     * @param input Input array to be remapped.
     * @return std::array<NDArray<T, 2>, N> Remapped arrays for each strixel
     * group.
     * @throws std::runtime_error If the input shape does not match the user ROI
     * shape.
     * @note This overload assumes that the user ROI has already been set and
     * the map calculated using `calculate_map()`.
     */
    template <typename T>
    std::array<NDArray<T, 2>, M> operator()(const NDView<T, 2> input) const;

    /**
     * @brief Apply the strixel-to-pixel remapping to an input array.
     * @param input Input array to be remapped.
     * @param output Preallocated array to store the remapped results for each
     * strixel group.
     * @throws std::runtime_error If the output shape does not match the user
     * ROI shape.
     * @note This overload assumes that the user ROI has already been set and
     * the map calculated using `calculate_map()`.
     */
    template <typename T>
    void operator()(const NDView<T, 2> input,
                    std::array<NDArray<T, 2>, M> &output) const;

    std::array<defs::StrixelGroupToPixelMap, M> get_group_maps() const {
        return m_group_maps;
    }

    const std::array<defs::StrixelGroupToPixelMap, M> &
    get_group_maps(std::size_t group_index) const {
        return m_group_maps;
    }

  protected:
    /**
     * @brief Build the strixel-to-pixel order map for one strixel group.
     * The strixel mapping is determined by the group's multiplicity and
     * modulo ordering. A reversed modulo ordering reverses the ordering
     * within each multiplicity chunk; it does not reverse the complete
     * strixel column ordering.
     *
     * @param group_config Configuration of the strixel group to be mapped.
     * @param pixel Sensor pixel geometry to which the group
     *              is connected.
     * @param placement Sensor placement and orientation on the module.
     * @param roi_user User-specified ROI in the module's native coordinate
     * system.
     * @param bond_shift Physical bonding shift in x and y directions defined in
     * the sensor's native coordinate system (before it is oriented on the
     * module).
     * @return A StrixelGroupToPixelMap describing the mapping from strixel
     * coordinates to pixel indices in the user-provided ROI.
     *      - map(strixel_row,strixel_col) = pixel_index_in_user_roi
     *      - Invalid or unmapped strixel positions are initialized to -1.
     * @throws std::logic_error For negative or zero strixel multiplicity.
     * @throws std::logic_error If the group ROI width is not divisible by
     *                          the strixel multiplicity.
     */
    defs::StrixelGroupToPixelMap
    strixel_to_pixel_map(defs::GroupConfig const &group_config);

    /**
     * @brief Applies all the group maps to the input.
     * @param input Original array
     * @return std::array<NDArray<T, 2>, N> Remapped arrays for each group
     * @throws std::invalid_argument If the output shape does not match the
     * order map shape for any group.
     */
    template <typename T>
    std::array<NDArray<T, 2>, M> apply_remap(const NDView<T, 2> input) const;

    /**
     * @brief Applies a given remapping rule to an input array.
     * @param input Original array
     * @param order_map Rule for remapping (e.g. the output of a map generator)
     * @param output Remapped array
     */
    template <typename T>
    void apply_group_remap(const NDView<T, 2> input, NDView<T, 2> output,
                           const NDView<const ssize_t, 2> order_map) const;

  protected:
    InclusiveROI m_user_roi{};
    std::array<defs::StrixelGroupToPixelMap, M>
        m_group_maps{}; // TODO: do we need to store effective ROI?
    SensorConfig<N> m_sensorconfig{};

  private:
    SensorModulePlacement m_module_placement{};
    BondShift m_bond_shift{};
};

template <std::size_t N, std::size_t M>
StrixelPixelMap<N, M>::StrixelPixelMap(
    const SensorConfig<N> &sensor_config,
    const SensorModulePlacement &module_placement, const BondShift &bond_shift)
    : m_sensorconfig(sensor_config), m_module_placement(module_placement),
      m_bond_shift(bond_shift) {}

template <std::size_t N, std::size_t M>
void StrixelPixelMap<N, M>::calculate_map(const ROI &user_roi) {
    m_user_roi = toInclusiveROI(user_roi);

    for (size_t i = 0; i < N; ++i) {
        m_group_maps[i] = strixel_to_pixel_map(m_sensorconfig.group_configs[i]);
    }
}

template <std::size_t N, std::size_t M>
template <typename T>
std::array<NDArray<T, 2>, M>
StrixelPixelMap<N, M>::operator()(const ROI &user_roi,
                                  const NDView<T, 2> input) {
    if (m_user_roi.is_empty()) {
        calculate_map(user_roi);
        return apply_remap(input);
    } else if (toInclusiveROI(user_roi) != m_user_roi) {
        calculate_map(user_roi);
        return apply_remap(input);
    } else {
        return apply_remap(input);
    }
}

template <std::size_t N, std::size_t M>
template <typename T>
std::array<NDArray<T, 2>, M>
StrixelPixelMap<N, M>::operator()(const NDView<T, 2> input) const {
    if (input.shape() !=
        std::array<ssize_t, 2>{m_user_roi.height(), m_user_roi.width()}) {
        throw std::runtime_error(
            fmt::format("shape mismatch between input and map: input shape = "
                        "({},{}), expected shape = ({},{})",
                        input.shape()[0], input.shape()[1], m_user_roi.height(),
                        m_user_roi.width()));
    }

    return apply_remap(input);
}

template <std::size_t N, std::size_t M>
template <typename T>
void StrixelPixelMap<N, M>::operator()(
    const NDView<T, 2> input, std::array<NDArray<T, 2>, M> &output) const {

    if (input.shape() !=
        std::array<ssize_t, 2>{m_user_roi.height(), m_user_roi.width()}) {
        throw std::runtime_error(
            fmt::format("shape mismatch between input and map: input shape = "
                        "({},{}), expected shape = ({},{})",
                        input.shape()[0], input.shape()[1], m_user_roi.height(),
                        m_user_roi.width()));
    }

    if (output.size() != m_group_maps.size()) {
        throw std::runtime_error(
            "output array size does not match number of group maps");
    }

    for (size_t i = 0; i < m_group_maps.size(); ++i) {
        apply_group_remap(input, output[i].view(), m_group_maps[i].map.view());
    }
}

template <std::size_t N, std::size_t M>
template <typename T>
std::array<NDArray<T, 2>, M>
StrixelPixelMap<N, M>::apply_remap(const NDView<T, 2> input) const {

    // TODO: maybe vector is better - empty ROIs - write tests !!!
    std::array<NDArray<T, 2>, M> outputs;

    for (size_t i = 0; i < m_group_maps.size(); ++i) {
        outputs[i] = NDArray<T, 2>{m_group_maps[i].map.shape()};
        apply_group_remap(input, outputs[i].view(), m_group_maps[i].map.view());
    }

    return outputs;
}

template <std::size_t N, std::size_t M>
template <typename T>
void StrixelPixelMap<N, M>::apply_group_remap(
    const NDView<T, 2> input, NDView<T, 2> output,
    const NDView<const ssize_t, 2> order_map) const {

    if (output.shape() != order_map.shape()) {
        throw std::invalid_argument(
            "ApplyRemap: output shape does not match order map shape");
    }

    const auto nrows = order_map.shape(0);
    const auto ncols = order_map.shape(1);

    for (ssize_t row = 0; row < nrows; ++row) {
        for (ssize_t col = 0; col < ncols; ++col) {

            auto flat_index = order_map(row, col);

            // Intentionally not-mapped pixel in order_map (e.g. guard ring
            // pixels)
            if (flat_index < 0) {
                output(row, col) = T{};
                continue;
            }

            // Corrupt map, must throw
            if (flat_index >= input.size()) {
                throw std::runtime_error(
                    "ApplyRemap: order map contains an invalid pixel index.");
            }

            // Correctly mapped pixel
            output(row, col) = input[flat_index];
        }
    }
}

template <std::size_t N, std::size_t M>
defs::StrixelGroupToPixelMap StrixelPixelMap<N, M>::strixel_to_pixel_map(
    defs::GroupConfig const &group_config) {

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
    const InclusiveROI roi_user_local = inclusiveroi::geom::rebaseROI(
        m_user_roi, m_module_placement.placement_on_module);
    LOG(logDEBUG)
        << "aare::remap::algo::strixel_to_pixel_map: Transformed user ROI: "
        << roi_user_local << std::endl;

    LOG(logDEBUG) << "aare::remap::algo::strixel_to_pixel_map: Group ROI "
                     "before transformation (as in global config)"
                  << group_config.placement_on_sensor << '\n';

    // -- 2) Apply the physical bond shift first, sensor rotation second.
    const InclusiveROI roi_group = detail::update_pixel_group_placement(
        group_config.placement_on_sensor, m_sensorconfig.pixel, m_bond_shift,
        m_module_placement.rotation);

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

} // namespace aare::remap
