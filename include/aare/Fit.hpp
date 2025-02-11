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

} // namespace func



/**
 * @brief Fit a 1D Gaussian to data.
 * @param data data to fit
 * @param x x values
 */
NDArray<double, 1> fit_gaus(NDView<double, 1> x, NDView<double, 1> y);


/**
 * @brief Fit a 1D Gaussian to each pixel. Data layout [row, col, values]
 * @param x x values
 * @param y y vales, layout [row, col, values]
 */
NDArray<double, 3> fit_gaus(NDView<double, 1> x, NDView<double, 3> y);


/**
 * @brief Fit a 1D Gaussian with error estimates
 * @param x x values
 * @param y y vales, layout [row, col, values]
 * @param y_err error in y, layout [row, col, values]
 */
void fit_gaus(NDView<double, 1> x, NDView<double, 1> y, NDView<double, 1> y_err,
              NDView<double, 1> par_out, NDView<double, 1> par_err_out);

/**
 * @brief Fit a 1D Gaussian to each pixel with error estimates. Data layout [row, col, values]
 * @param x x values
 * @param y y vales, layout [row, col, values]
 * @param y_err error in y, layout [row, col, values]
 */
void fit_gaus(NDView<double, 1> x, NDView<double, 3> y, NDView<double, 3> y_err,
              NDView<double, 3> par_out, NDView<double, 3> par_err_out);


NDArray<double, 1> fit_pol1(NDView<double, 1> x, NDView<double, 1> y);

NDArray<double, 3> fit_pol1(NDView<double, 1> x, NDView<double, 3> y);

void fit_pol1(NDView<double, 1> x, NDView<double, 1> y,
                NDView<double, 1> y_err, NDView<double, 1> par_out,
                NDView<double, 1> par_err_out);

//TODO! not sure we need to offer the different version in C++
void fit_pol1(NDView<double, 1> x, NDView<double, 3> y,
                NDView<double, 3> y_err, NDView<double, 3> par_out,
                NDView<double, 3> par_err_out);

} // namespace aare