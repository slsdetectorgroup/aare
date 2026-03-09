#pragma once

#include "aare/RemapDefs.hpp"

namespace aare::remap::algo {
// Is it better to pass defs::SensorGroupConfig const& and return a copy?
void apply_rotation_shift(defs::SensorGroupConfig &cfg,
                          defs::BondShift bond_shift, defs::Rotation rot);
defs::StrixelGroupToPixelMap
strixel_to_pixel_map(defs::SensorGroupConfig, defs::SensorPlacement,
                     InclusiveROI user_roi,
                     defs::BondShift bond_shift = {0, 0});
std::vector<defs::StrixelGroupToPixelMap>
strixel_to_pixel_maps(defs::SensorConfig, defs::SensorPlacement,
                      InclusiveROI user_roi,
                      defs::BondShift bond_shift = {0, 0});

/**
 *  Public API:
 *  Applies a given remapping rule to an input array.
 *
 * \param input Original array
 * \param order_map Rule for remapping
 * \param output Remapped array
 */
template <typename T>
void ApplyRemap(NDView<T, 2> const &input, NDArray<ssize_t, 2> const &order_map,
                NDArray<T, 2> &output) {
    for (size_t row = 0; row < order_map.shape(0); ++row) {
        for (size_t col = 0; col < order_map.shape(1); ++col) {
            auto flat_index = order_map(row, col);
            if (flat_index >= 0 &&
                static_cast<size_t>(flat_index) < input.size()) {
                T const &value = input[flat_index];
                output(row, col) = value;
                // output(row, col) = static_cast<T>(input[flat_index]);
            } else {
                output(row, col) = static_cast<T>(0); // or nan?
            }
        }
    }
}
} // namespace aare::remap::algo