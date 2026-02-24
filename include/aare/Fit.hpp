// SPDX-License-Identifier: MPL-2.0
#pragma once

#include <cmath>
#include <fmt/core.h>
#include <vector>

#include "aare/NDArray.hpp"

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

/**
 * @brief Estimate the initial parameters for a Gaussian fit
 */
std::array<double, 3> gaus_init_par(const NDView<double, 1> x,
                                    const NDView<double, 1> y);

std::array<double, 2> pol1_init_par(const NDView<double, 1> x,
                                    const NDView<double, 1> y);

std::array<double, 6> scurve_init_par(const NDView<double, 1> x,
                                      const NDView<double, 1> y);
std::array<double, 6> scurve2_init_par(const NDView<double, 1> x,
                                       const NDView<double, 1> y);

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
 * @brief Fit a 1D Gaussian using Minuit2 (finite-difference gradients).
 *
 * Model: f(x) = A * exp(-(x - mu)^2 / (2 * sigma^2))
 *
 * @param x Scan point values.
 * @param y Measured values at each scan point.
 * @param y_err Per-point standard deviations. Points with y_err == 0 are skipped.
 * @return Shape {3}: [A, mu, sigma]. All zeros if fit fails.
 */
NDArray<double, 1> fit_gaus_minuit(NDView<double, 1> x,
                                   NDView<double, 1> y,
                                   NDView<double, 1> y_err);

/**
 * @brief Fit a 1D Gaussian using Minuit2 (analytic gradients).
 *
 * Same model as fit_gaus_minuit() but with analytic chi-squared gradients
 * and optional MnHesse error estimation.
 *
 * @param x Scan point values.
 * @param y Measured values at each scan point.
 * @param y_err Per-point standard deviations. Points with y_err == 0 are skipped.
 * @param compute_errors If true, run MnHesse and append 1-sigma errors.
 * @return Shape {3}: [A, mu, sigma], or {6}: [A, mu, sigma, err_A, err_mu,
 *         err_sigma] if compute_errors is true. All zeros if fit fails.
 */
NDArray<double, 1> fit_gaus_minuit_grad(NDView<double, 1> x,
                                        NDView<double, 1> y,
                                        NDView<double, 1> y_err,
                                        bool compute_errors = true);
                                        
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
} // namespace aare