#pragma once

#include "aare/StrixelPixelRemapDefs.hpp"

namespace aare::remap::algo {

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

/**
 * @brief Build the strixel-to-pixel order map for one strixel group.
 * The strixel mapping is determined by the group's multiplicity and
 * modulo ordering. A reversed modulo ordering reverses the ordering
 * within each multiplicity group; it does not reverse the complete
 * strixel column ordering.
 *
 * @param group_config Configuration of the strixel group to be mapped.
 * @param pixel Sensor pixel geometry to which the group
 *              is connected.
 * @param placement Sensor placement and orientation on the module.
 * @param roi_user User-specified ROI in the module's native coordinate system.
 * @param bond_shift Physical bonding shift in x and y directions.
 * @return A StrixelGroupToPixelMap describing the mapping from strixel
 * coordinates to pixel indices in the user-provided ROI.
 *      - map(strixel_row,strixel_col) = pixel_index_in_user_roi
 *      - Invalid or unmapped strixel positions are initialized to -1.
 * @throws std::logic_error For negative or zero strixel multiplicity.
 * @throws std::logic_error If the group ROI width is not divisible by
 *                          the strixel multiplicity.
 */
defs::StrixelGroupToPixelMap
strixel_to_pixel_map(defs::GroupConfig const &group_config,
                     defs::SensorPixelGeometry const &pixel,
                     defs::SensorModulePlacement const &placement,
                     InclusiveROI const &user_roi,
                     defs::BondShift bond_shift = {0, 0});

/**
 * @brief Build the strixel-to-pixel order maps for all strixel groups in a
 * sensor.
 * @param sensor_config Configuration of the sensor, including all
 * configurations of the strixel groups.
 * @param placement Sensor placement and orientation on the module.
 * @param roi_user User-specified ROI in the module's native coordinate system.
 * @param bond_shift Bonding shift in x and y directions.
 * @return A vector of StrixelGroupToPixelMap containing the mappings for all
 * strixel groups.
 */
std::vector<defs::StrixelGroupToPixelMap> strixel_to_pixel_maps(
    defs::SensorConfig const &, defs::SensorModulePlacement const &,
    InclusiveROI const &user_roi, defs::BondShift bond_shift = {0, 0});

/**
 *  Public API:
 *  Applies a given remapping rule to an input array.
 *
 * \param input Original array
 * \param order_map Rule for remapping
 * \param output Remapped array
 */
template <typename T>
void ApplyRemap(NDView<T, 2> input, NDView<ssize_t, 2> order_map,
                NDArray<T, 2> &output) {

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
            if (static_cast<size_t>(flat_index) >= input.size()) {
                throw std::runtime_error(
                    "ApplyRemap: order map contains an invalid pixel index.");
            }

            // Correctly mapped pixel
            output(row, col) = input[flat_index];
            // Long version
            // T const &value = input[flat_index];
            // output(row, col) = value;
        }
    }
}
} // namespace aare::remap::algo