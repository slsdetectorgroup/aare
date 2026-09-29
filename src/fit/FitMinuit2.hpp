// SPDX-License-Identifier: MPL-2.0
#pragma once

#include "aare/FitModel.hpp"
#include "aare/NDView.hpp"
#include <array>

// Minuit2 backends (Migrad and Fumili) of the pixel fit. FitMinuit2.cpp is
// the only translation unit that includes Minuit2 headers; it defines and
// explicitly instantiates the function below for every supported model.

namespace aare::detail {

/**
 * @brief Fit one pixel with the Minuit2 minimizer selected by the model.
 *
 * @param start    Starting values, see start_values() in FitHelpers.hpp.
 * @param par_out  Receives npar fitted values; zeros on failure.
 * @param err_out  Receives npar errors when non-null; zeros on failure.
 *                 Fixed parameters and parameters on a limit report 0.
 * @param chi2     Receives the chi-squared at the minimum; 0 on failure.
 * @return true when Minuit2 reports a valid minimum.
 */
template <typename Model>
bool fit_pixel_minuit2(const FitModel<Model> &model, NDView<double, 1> x,
                       NDView<double, 1> y, NDView<double, 1> y_err,
                       const std::array<double, Model::npar> &start,
                       double *par_out, double *err_out, double &chi2);

} // namespace aare::detail
