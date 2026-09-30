// SPDX-License-Identifier: MPL-2.0
#pragma once

#include "aare/FitModel.hpp"
#include "aare/NDView.hpp"
#include <algorithm>
#include <array>
#include <stdexcept>

// Helpers shared by the built-in solvers and the Minuit2 backend so that all
// apply the same start-value precedence, budget and shape checks.

namespace aare::detail {

/**
 * @brief Starting values for one pixel.
 *
 * Model::estimate_par supplies data-driven estimates. User start values and
 * fixed values take precedence, and free parameters are clamped into their
 * limits.
 */
template <typename Model>
std::array<double, Model::npar> start_values(const FitModel<Model> &model,
                                             NDView<double, 1> x,
                                             NDView<double, 1> y) {
    auto start = Model::estimate_par(x, y);
    for (std::size_t k = 0; k < Model::npar; ++k) {
        const auto idx = static_cast<unsigned int>(k);
        if (model.is_user_fixed(idx) || model.is_user_start(idx))
            start[k] = model.value(idx);
        if (!model.is_user_fixed(idx))
            start[k] = std::clamp(start[k], model.lower_limit(idx),
                                  model.upper_limit(idx));
    }
    return start;
}

/**
 * @brief Evaluation budget of the built-in minimizers. As in Minuit2,
 * max_calls = 0 selects the default 200 + 100 * npar + 5 * npar^2.
 */
template <typename Model> int evaluation_budget(const FitModel<Model> &model) {
    constexpr int npar = static_cast<int>(Model::npar);
    return model.max_calls() > 0 ? static_cast<int>(model.max_calls())
                                 : 200 + 100 * npar + 5 * npar * npar;
}

/** @brief Validate the array sizes passed to fit_pixel. */
inline void check_fit_pixel_shapes(NDView<double, 1> x, NDView<double, 1> y,
                                   NDView<double, 1> y_err) {
    if (x.size() != y.size())
        throw std::runtime_error("fit_pixel: x.size() must match y.size().");
    if (y_err.size() > 0 && y_err.size() != y.size())
        throw std::runtime_error(
            "fit_pixel: y and y_err must have identical size.");
}

/** @brief Validate the array shapes passed to fit_3d. */
template <typename Model>
void check_fit_3d_shapes(NDView<double, 1> x, NDView<double, 3> y,
                         NDView<double, 3> y_err, NDView<double, 3> par_out,
                         NDView<double, 3> err_out,
                         NDView<double, 2> chi2_out) {
    constexpr auto npar = static_cast<ssize_t>(Model::npar);

    if (x.size() != y.shape(2))
        throw std::runtime_error("fit_3d: x.size() must match y.shape(2).");

    if (par_out.shape(0) != y.shape(0) || par_out.shape(1) != y.shape(1) ||
        par_out.shape(2) != npar)
        throw std::runtime_error("par_out must have shape [rows, cols, npar].");

    if (chi2_out.shape(0) != y.shape(0) || chi2_out.shape(1) != y.shape(1))
        throw std::runtime_error("chi2_out must have shape [rows, cols].");

    if (y_err.size() > 0 &&
        (y.shape(0) != y_err.shape(0) || y.shape(1) != y_err.shape(1) ||
         y.shape(2) != y_err.shape(2)))
        throw std::runtime_error(
            "fit_3d: y and y_err must have identical shape.");

    if (err_out.size() > 0 &&
        (err_out.shape(0) != y.shape(0) || err_out.shape(1) != y.shape(1) ||
         err_out.shape(2) != npar))
        throw std::runtime_error("err_out must have shape [rows, cols, npar].");
}

} // namespace aare::detail
