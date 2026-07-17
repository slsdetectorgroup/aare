#pragma once

#include "aare/RemapDefs.hpp"

namespace aare::remap::algo {

constexpr defs::Rotation flip(defs::Rotation r) noexcept {
    switch (r) {
    case defs::Rotation::Identity:
        return defs::Rotation::Rotate180;
    case defs::Rotation::Rotate180:
        return defs::Rotation::Identity;
    }

    assert(false && "Invalid Rotation passed to flip");
    return defs::Rotation::Identity; // Unreachable; satisfies compiler
}

// Is it better to pass defs::GroupConfig const& and return a copy?
void apply_rotation_shift(defs::GroupConfig &,
                          defs::SensorPixelGeometry const &, defs::BondShift,
                          defs::Rotation);

defs::StrixelGroupToPixelMap strixel_to_pixel_map(
    defs::GroupConfig const &, defs::SensorPixelGeometry const &,
    defs::SensorPlacement const &, InclusiveROI const &user_roi,
    defs::BondShift bond_shift = {0, 0});

std::vector<defs::StrixelGroupToPixelMap>
strixel_to_pixel_maps(defs::SensorConfig const &, defs::SensorPlacement const &,
                      InclusiveROI const &user_roi,
                      defs::BondShift bond_shift = {0, 0});

defs::StrixelGroupToPixelMap
combine_maps(std::vector<defs::StrixelGroupToPixelMap> const &,
             std::vector<int> const &);

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