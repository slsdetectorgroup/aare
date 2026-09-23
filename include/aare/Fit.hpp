// SPDX-License-Identifier: MPL-2.0
#pragma once

#include "aare/FitModel.hpp"
#include "aare/NDArray.hpp"

namespace aare {

// ---------------------------------------------------------------------------
// Minuit2-based pixel fitting.
// Template bodies and explicit instantiations live in src/Fit.cpp.
// ---------------------------------------------------------------------------

/**
 * @brief Fit a single pixel's data using Minuit2.
 *
 * User-precedence rules:
 *   - Fixed parameters: untouched (value and fixed flag preserved from model).
 *   - User-set start:   value preserved, step size auto-filled.
 *   - Neither:          both value and step size auto-filled from data.
 *
 * @tparam Model  Model struct (Gaussian, RisingScurve, …).
 *
 * @param model   The FitModel configuration (read-only).
 * @param x       Scan points.
 * @param y       Measured values for this pixel.
 * @param y_err   Per-point uncertainties (empty view -> unweighted fit).
 *
 * @return NDArray<double,1> of size:
 *   - compute_errors: [p0..pN, err0..errN, chi2]  -> 2*npar + 1
 *   - otherwise:      [p0..pN, chi2]              -> npar + 1
 */
template <typename Model>
NDArray<double, 1> fit_pixel(const FitModel<Model> &model, NDView<double, 1> x,
                             NDView<double, 1> y, NDView<double, 1> y_err);

// Overload: uncertainties not provided
template <typename Model>
NDArray<double, 1> fit_pixel(const FitModel<Model> &model, NDView<double, 1> x,
                             NDView<double, 1> y);

/**
 * @brief Fit all pixels in a 3D data cube (rows x cols x n_scan).
 *
 * @tparam Model  Model struct.
 *
 * @param model      Fit configuration shared by all pixels.
 * @param x          Scan points, shape `(n_scan)`.
 * @param y          Measured values, shape `(rows, cols, n_scan)`.
 * @param y_err      Uncertainties, same shape as y, or empty for unweighted
 * fits.
 * @param par_out    Output parameters, shape `(rows, cols, npar)`.
 * @param err_out    Output parameter errors, shape `(rows, cols, npar)`, if
 * used.
 * @param chi2_out   Output chi-squared / objective values, shape `(rows,
 * cols)`.
 * @param n_threads  Number of threads used to split rows.
 */
template <typename Model>
void fit_3d(const FitModel<Model> &model, NDView<double, 1> x,
            NDView<double, 3> y, NDView<double, 3> y_err,
            NDView<double, 3> par_out, NDView<double, 3> err_out,
            NDView<double, 2> chi2_out, int n_threads);

} // namespace aare
