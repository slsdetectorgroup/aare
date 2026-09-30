// SPDX-License-Identifier: MPL-2.0
#pragma once

// Data-driven Minuit step sizes for the optional Minuit2 backend. These
// estimators used to live in aare/Models.hpp; the built-in solver does not
// need them.

#include "aare/Models.hpp"
#include "aare/NDView.hpp"
#include <algorithm>
#include <array>
#include <cmath>

namespace aare::minuit2 {

/**
 * @brief Compute data-range statistics used by the step-size estimators.
 *
 * Model-independent, called once per pixel.
 */
inline void compute_ranges(NDView<double, 1> x, NDView<double, 1> y,
                           double &x_range, double &y_range,
                           double &slope_scale) {
    const auto [x_min, x_max] = std::minmax_element(x.begin(), x.end());
    const auto [y_min, y_max] = std::minmax_element(y.begin(), y.end());

    x_range = std::max(*x_max - *x_min, 1e-9);
    y_range = std::max(*y_max - *y_min, 1e-9);

    slope_scale = std::max(y_range / x_range, 1e-9);
}

/** @brief Per-model Minuit step sizes, specialised for every fit model. */
template <typename Model> struct Steps;

template <> struct Steps<model::Pol1> {
    static void compute(const std::array<double, 2> &start,
                        [[maybe_unused]] double x_range, double y_range,
                        double slope_scale, std::array<double, 2> &steps) {
        steps[0] = std::max(0.1 * std::abs(start[0]), 0.1 * y_range);
        steps[1] = 0.1 * slope_scale;
    }
};

template <> struct Steps<model::Pol2> {
    static void compute(const std::array<double, 3> &start, double x_range,
                        double y_range, double slope_scale,
                        std::array<double, 3> &steps) {
        steps[0] = std::max(0.1 * std::abs(start[0]), 0.1 * y_range);
        steps[1] = 0.1 * slope_scale;
        steps[2] = 0.1 * slope_scale / std::max(x_range, 1e-12);
    }
};

template <> struct Steps<model::Gaussian> {
    static void compute(const std::array<double, 3> &start, double x_range,
                        double y_range, double /*slope_scale*/,
                        std::array<double, 3> &steps) {
        steps[0] = std::max(0.1 * std::abs(start[0]), 0.1 * y_range);
        steps[1] = 0.05 * x_range;
        steps[2] = 0.05 * x_range;
    }
};

template <> struct Steps<model::GaussianErfcPlateau> {
    static void compute(const std::array<double, 4> &start, double x_range,
                        double y_range, double /*slope_scale*/,
                        std::array<double, 4> &steps) {
        steps[0] = std::max(0.1 * std::abs(start[0]), 0.1 * y_range);
        steps[1] = std::max(0.1 * std::abs(start[1]), 0.1 * y_range);
        steps[2] = 0.05 * x_range;
        steps[3] = 0.05 * x_range;
    }
};

template <> struct Steps<model::GaussianChargeSharing> {
    static void compute(const std::array<double, 6> &start, double x_range,
                        double y_range, double slope_scale,
                        std::array<double, 6> &steps) {
        steps[0] = std::max(0.1 * std::abs(start[0]), 0.1 * y_range);
        steps[1] = 0.1 * slope_scale;
        steps[2] = 0.05 * x_range;
        steps[3] = 0.05 * x_range;
        steps[4] = std::max(0.1 * std::abs(start[4]), 0.1 * y_range);
        steps[5] = std::max(0.1 * std::abs(start[5]), 0.01);
    }
};

template <> struct Steps<model::GaussianChargeSharingKb> {
    static void compute(const std::array<double, 8> &start, double x_range,
                        double y_range, double slope_scale,
                        std::array<double, 8> &steps) {
        steps[0] = std::max(0.1 * std::abs(start[0]), 0.1 * y_range);
        steps[1] = 0.1 * slope_scale;
        steps[2] = 0.05 * x_range;
        steps[3] = 0.05 * x_range;
        steps[4] = std::max(0.1 * std::abs(start[4]), 0.1 * y_range);
        steps[5] = std::max(0.1 * std::abs(start[5]), 0.01);
        steps[6] = std::max(0.01 * std::abs(start[6]), 1e-3);
        steps[7] = std::max(0.1 * std::abs(start[7]), 0.01);
    }
};

template <> struct Steps<model::RisingScurve> {
    static void compute(const std::array<double, 6> &start, double x_range,
                        double y_range, double slope_scale,
                        std::array<double, 6> &steps) {
        steps[0] = std::max(0.1 * std::abs(start[0]), 0.1 * y_range);
        steps[1] = 0.1 * slope_scale;
        steps[2] = 0.05 * x_range;
        steps[3] = 0.05 * x_range;
        steps[4] = std::max(0.1 * std::abs(start[4]), 0.1 * y_range);
        steps[5] = 0.1 * slope_scale;
    }
};

template <> struct Steps<model::FallingScurve> {
    static void compute(const std::array<double, 6> &start, double x_range,
                        double y_range, double slope_scale,
                        std::array<double, 6> &steps) {
        Steps<model::RisingScurve>::compute(start, x_range, y_range,
                                            slope_scale, steps);
    }
};

} // namespace aare::minuit2
