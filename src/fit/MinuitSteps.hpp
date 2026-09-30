// SPDX-License-Identifier: MPL-2.0
#pragma once

// Data-driven Minuit step sizes for the Minuit2 backend. The built-in
// solvers do not need them.

#include "aare/Models.hpp"
#include "aare/NDView.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>

namespace aare::minuit2 {

/** @brief What a parameter measures, which sets the size of its first step. */
enum class Scale {
    Height,    // a y value: offset or amplitude
    Slope,     // dy/dx
    Curvature, // d2y/dx2
    Position,  // an x value: position or width
    Fraction,  // dimensionless, relative to another parameter
    Ratio,     // dimensionless and close to one
};

/** @brief The scale of every parameter of a fit model. */
template <typename Model> struct Scales;

template <> struct Scales<model::Pol1> {
    static constexpr std::array<Scale, 2> value = {Scale::Height, Scale::Slope};
};

template <> struct Scales<model::Pol2> {
    static constexpr std::array<Scale, 3> value = {Scale::Height, Scale::Slope,
                                                   Scale::Curvature};
};

template <> struct Scales<model::Gaussian> {
    static constexpr std::array<Scale, 3> value = {
        Scale::Height, Scale::Position, Scale::Position};
};

template <> struct Scales<model::GaussianErfcPlateau> {
    static constexpr std::array<Scale, 4> value = {
        Scale::Height, Scale::Height, Scale::Position, Scale::Position};
};

template <> struct Scales<model::GaussianChargeSharing> {
    static constexpr std::array<Scale, 6> value = {
        Scale::Height,   Scale::Slope,  Scale::Position,
        Scale::Position, Scale::Height, Scale::Fraction};
};

template <> struct Scales<model::GaussianChargeSharingKb> {
    static constexpr std::array<Scale, 8> value = {
        Scale::Height, Scale::Slope,    Scale::Position, Scale::Position,
        Scale::Height, Scale::Fraction, Scale::Ratio,    Scale::Fraction};
};

template <> struct Scales<model::RisingScurve> {
    static constexpr std::array<Scale, 6> value = {
        Scale::Height,   Scale::Slope,  Scale::Position,
        Scale::Position, Scale::Height, Scale::Slope};
};

template <>
struct Scales<model::FallingScurve> : Scales<model::RisingScurve> {};

/** @brief Extent of one pixel's data, which the steps are scaled to. */
struct Ranges {
    double x;
    double y;
    double slope; // y / x
};

inline Ranges compute_ranges(NDView<double, 1> x, NDView<double, 1> y) {
    const auto [x_min, x_max] = std::minmax_element(x.begin(), x.end());
    const auto [y_min, y_max] = std::minmax_element(y.begin(), y.end());

    const double x_range = std::max(*x_max - *x_min, 1e-9);
    const double y_range = std::max(*y_max - *y_min, 1e-9);
    return {x_range, y_range, std::max(y_range / x_range, 1e-9)};
}

/** @brief Step of a parameter of the given scale that starts at start. */
inline double step(Scale scale, double start, const Ranges &range) {
    switch (scale) {
    case Scale::Height:
        return std::max(0.1 * std::abs(start), 0.1 * range.y);
    case Scale::Slope:
        return 0.1 * range.slope;
    case Scale::Curvature:
        return 0.1 * range.slope / std::max(range.x, 1e-12);
    case Scale::Position:
        return 0.05 * range.x;
    case Scale::Fraction:
        return std::max(0.1 * std::abs(start), 0.01);
    case Scale::Ratio:
        return std::max(0.01 * std::abs(start), 1e-3);
    }
    return 0.0;
}

/** @brief Minuit step sizes of all parameters for one pixel. */
template <typename Model>
std::array<double, Model::npar>
steps(const std::array<double, Model::npar> &start, NDView<double, 1> x,
      NDView<double, 1> y) {
    const Ranges range = compute_ranges(x, y);
    std::array<double, Model::npar> out{};
    for (std::size_t k = 0; k < Model::npar; ++k)
        out[k] = step(Scales<Model>::value[k], start[k], range);
    return out;
}

} // namespace aare::minuit2
