// SPDX-License-Identifier: MPL-2.0
#pragma once

#include <cmath>
#include <vector>

#include "aare/FitModel.hpp"
#include "aare/NDArray.hpp"
#include "aare/utils/par.hpp"
#include "aare/utils/task.hpp"

namespace aare {

namespace func {
double gaus(const double x, const double *par);
NDArray<double, 1> gaus(NDView<double, 1> x, NDView<double, 1> par);

double pol1(const double x, const double *par);
NDArray<double, 1> pol1(NDView<double, 1> x, NDView<double, 1> par);

double scurve(const double x, const double *par);
NDArray<double, 1> scurve(NDView<double, 1> x, NDView<double, 1> par);

double scurve2(const double x, const double *par);
NDArray<double, 1> scurve2(NDView<double, 1> x, NDView<double, 1> par);

} // namespace func

static constexpr int DEFAULT_NUM_THREADS = 4;

/**
 * @brief Fit a 1D Gaussian to data.
 * @param data data to fit
 * @param x x values
 */
NDArray<double, 1> fit_gaus(NDView<double, 1> x, NDView<double, 1> y);

/**
 * @brief Fit a 1D Gaussian to each pixel. Data layout [row, col, values]
 * @param x x values
 * @param y y values, layout [row, col, values]
 * @param n_threads number of threads to use
 */
NDArray<double, 3> fit_gaus(NDView<double, 1> x, NDView<double, 3> y,
                            int n_threads = DEFAULT_NUM_THREADS);

/**
 * @brief Fit a 1D Gaussian with error estimates
 * @param x x values
 * @param y y values, layout [row, col, values]
 * @param y_err error in y, layout [row, col, values]
 * @param par_out output parameters
 * @param par_err_out output error parameters
 */
void fit_gaus(NDView<double, 1> x, NDView<double, 1> y, NDView<double, 1> y_err,
              NDView<double, 1> par_out, NDView<double, 1> par_err_out,
              double &chi2);

/**
 * @brief Fit a 1D Gaussian to each pixel with error estimates. Data layout
 * [row, col, values]
 * @param x x values
 * @param y y values, layout [row, col, values]
 * @param y_err error in y, layout [row, col, values]
 * @param par_out output parameters, layout [row, col, values]
 * @param par_err_out output parameter errors, layout [row, col, values]
 * @param n_threads number of threads to use
 */
void fit_gaus(NDView<double, 1> x, NDView<double, 3> y, NDView<double, 3> y_err,
              NDView<double, 3> par_out, NDView<double, 3> par_err_out,
              NDView<double, 2> chi2_out, int n_threads = DEFAULT_NUM_THREADS);

NDArray<double, 1> fit_pol1(NDView<double, 1> x, NDView<double, 1> y);

NDArray<double, 3> fit_pol1(NDView<double, 1> x, NDView<double, 3> y,
                            int n_threads = DEFAULT_NUM_THREADS);

void fit_pol1(NDView<double, 1> x, NDView<double, 1> y, NDView<double, 1> y_err,
              NDView<double, 1> par_out, NDView<double, 1> par_err_out,
              double &chi2);

// TODO! not sure we need to offer the different version in C++
void fit_pol1(NDView<double, 1> x, NDView<double, 3> y, NDView<double, 3> y_err,
              NDView<double, 3> par_out, NDView<double, 3> par_err_out,
              NDView<double, 2> chi2_out, int n_threads = DEFAULT_NUM_THREADS);

NDArray<double, 1> fit_scurve(NDView<double, 1> x, NDView<double, 1> y);
NDArray<double, 3> fit_scurve(NDView<double, 1> x, NDView<double, 3> y,
                              int n_threads);
void fit_scurve(NDView<double, 1> x, NDView<double, 1> y,
                NDView<double, 1> y_err, NDView<double, 1> par_out,
                NDView<double, 1> par_err_out, double &chi2);
void fit_scurve(NDView<double, 1> x, NDView<double, 3> y,
                NDView<double, 3> y_err, NDView<double, 3> par_out,
                NDView<double, 3> par_err_out, NDView<double, 2> chi2_out,
                int n_threads);

NDArray<double, 1> fit_scurve2(NDView<double, 1> x, NDView<double, 1> y);
NDArray<double, 3> fit_scurve2(NDView<double, 1> x, NDView<double, 3> y,
                               int n_threads);
void fit_scurve2(NDView<double, 1> x, NDView<double, 1> y,
                 NDView<double, 1> y_err, NDView<double, 1> par_out,
                 NDView<double, 1> par_err_out, double &chi2);
void fit_scurve2(NDView<double, 1> x, NDView<double, 3> y,
                 NDView<double, 3> y_err, NDView<double, 3> par_out,
                 NDView<double, 3> par_err_out, NDView<double, 2> chi2_out,
                 int n_threads);

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
