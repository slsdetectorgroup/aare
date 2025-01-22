#pragma once

#include <cmath>
#include <fmt/core.h>
#include <vector>

#include "aare/NDArray.hpp"

namespace aare {

namespace func {
double gauss(const double x, const double *par);
NDArray<double, 1> gauss(NDView<double, 1> x, NDView<double, 1> par);

double affine(const double x, const double *par);
NDArray<double, 1> affine(NDView<double, 1> x, NDView<double, 1> par);

} // namespace func

/**
 * @brief Fit a 1D Gaussian to each pixel. Data layout [row, col, values]
 * @param data data to fit
 * @param x x values
 */
NDArray<double, 3> fit_gaus(NDView<double, 3> data, NDView<double, 1> x);

/**
 * @brief Fit a 1D Gaussian to data.
 * @param data data to fit
 * @param x x values
 */
NDArray<double, 1> fit_gaus(NDView<double, 1> data, NDView<double, 1> x);

/**
 * @brief Fit a 1D gaus function to each pixel. Data layout [row, col, values]
 * @param data data to fit
 * @param x x values
 * @param data_err error in data
 */
NDArray<double, 3> fit_gaus(NDView<double, 3> data, NDView<double, 1> x, NDView<double, 3> data_err);

/**
 * @brief Fit a 1D Gaussian to data.
 * @param data data to fit
 * @param x x values
 * @param data_err error in data
 */
NDArray<double, 1> fit_gaus(NDView<double, 1> data, NDView<double, 1> x, NDView<double, 1> data_err);

/**
 * @brief Fit an affine function to each pixel. Data layout [row, col, values]
 * @param data data to fit
 * @param x x values
 * @param data_err error in data
 */
NDArray<double, 3> fit_affine(NDView<double, 3> data, NDView<double, 1> x, NDView<double, 3> data_err);

/**
 * @brief Fit an affine function to data.
 * @param data data to fit
 * @param x x values
 * @param data_err error in data
 */
NDArray<double, 1> fit_affine(NDView<double, 1> data, NDView<double, 1> x, NDView<double, 1> data_err);

} // namespace aare