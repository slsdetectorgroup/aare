// SPDX-License-Identifier: MPL-2.0
#pragma once

#include <cmath>
#include <fmt/core.h>
#include <vector>

#include "aare/utils/par.hpp"
#include "aare/utils/task.hpp"
#include "aare/NDArray.hpp"
#include "aare/Chi2.hpp"
#include "aare/FitModel.hpp"

#include "Minuit2/FunctionMinimum.h"
#include "Minuit2/MnMigrad.h"
#include "Minuit2/MnHesse.h"
#include "Minuit2/MnUserParameters.h"
#include "Minuit2/MnPrint.h"

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

// Minuit2 fit_pixel / fit_3d object based API

// _____________________________________________________________________
//
// fit_pixel — single-pixel minimisation
// _____________________________________________________________________

/**
 * @brief Fit a single pixel's data using Minuit2.
 *
 * The caller provides a thread-local clone of MnUserParameters so that
 * no heap allocation happens here (only SetValue/SetError stores).
 *
 * User-precedence rules:
 *   - Fixed parameters: untouched (value and fixed flag preserved from clone).
 *   - User-set start:   value preserved, step size auto-filled.
 *   - Neither:          both value and step size auto-filled from data.
 *
 * @tparam Model  Model struct (Gaussian, RisingScurve, …).
 * @tparam FCN    Chi2 functor type (Chi2Model1D or Chi2Model1DGrad instantiation).
 *
 * @param model       The FitModel configuration (read-only).
 * @param upar_local  Thread-local clone of model.upar().  Modified in place.
 * @param x           Scan points (shared across all pixels).
 * @param y           Measured values for this pixel.
 * @param y_err       Per-point uncertainties (empty view -> unweighted fit).
 *
 * @return NDArray<double,1> of size:
 *   - compute_errors: [p0..pN, err0..errN, chi2]  -> 2*npar + 1
 *   - otherwise:      [p0..pN, chi2]              -> npar + 1
 */
template<typename Model, typename FCN>
NDArray<double, 1> fit_pixel(const FitModel<Model>& model,
                            ROOT::Minuit2::MnUserParameters& upar_local,
                            NDView<double, 1> x,
                            NDView<double, 1> y,
                            NDView<double, 1> y_err) {

    constexpr std::size_t npar = Model::npar; 
    const bool want_errors = model.compute_errors();
    const ssize_t result_size = want_errors ? (2 * npar + 1) : (npar + 1);

    // ──── automatic parameter estimation ─────────────
    auto start = Model::estimate_par(x,y);
              
    // dead / degenerate pixel guard
    if (!Model::is_valid(std::vector<double>(start.begin(), start.end()))) {
        return NDArray<double, 1>({result_size}, 0.0);
    }

    // ──── data-range statistics for step sizes ─────────────
    double x_range, y_range, slope_scale;
    model::compute_ranges(x, y, x_range, y_range, slope_scale);
 
    std::array<double, npar> steps{};
    Model::compute_steps(start, x_range, y_range, slope_scale, steps);

    // ── apply auto-estimates respecting user precedence ─────────────
    for(std::size_t i = 0; i < npar; ++i){
        // fixed: do not touch at all
        if(model.is_user_fixed(i)){
            continue;
        }

        if(!model.is_user_start(i)){
            upar_local.SetValue(i, start[i]);
        }

        upar_local.SetError(i, steps[i]);
    }

    // ──── build functor ────────
    auto chi2 = (y_err.size() > 0) ? FCN(x, y, y_err) : FCN(x, y);

    // ──── run minimizer ────────
    ROOT::Minuit2::MnMigrad migrad(chi2, upar_local, model.strategy());
    ROOT::Minuit2::FunctionMinimum min = migrad(model.max_calls(), model.tolerance());

    if (!min.IsValid())
        return NDArray<double, 1>({result_size}, 0.0);

    // ──── pack results ────────
    if (want_errors) {
        ROOT::Minuit2::MnHesse hesse;
        hesse(chi2, min);
 
        const auto& values = min.UserState().Params();
        const auto& errors = min.UserState().Errors();
 
        NDArray<double, 1> result({result_size});
        for (std::size_t k = 0; k < npar; ++k) {
            result[k]        = values[k];
            result[npar + k] = errors[k];
        }
        result[2 * npar] = min.Fval();
        return result;
    }
 
    const auto& values = min.UserState().Params();
    NDArray<double, 1> result({result_size});
    for (std::size_t k = 0; k < npar; ++k)
        result[k] = values[k];
    result[npar] = min.Fval();
    return result;
}

// ── self-contained for 1D / standalone use ─────────
template <typename Model, typename FCN>
NDArray<double, 1> fit_pixel(const FitModel<Model>& model,
                             NDView<double, 1> x,
                             NDView<double, 1> y,
                             NDView<double, 1> y_err)
{
    auto upar_local = model.upar();
    return fit_pixel<Model, FCN>(model, upar_local, x, y, y_err);
}

// Overload: uncertainties not provided
template <typename Model, typename FCN>
NDArray<double, 1> fit_pixel(const FitModel<Model>& model,
                             NDView<double, 1> x,
                             NDView<double, 1> y)
{
    auto upar_local = model.upar();
    return fit_pixel<Model, FCN>(model, upar_local, x, y, NDView<double, 1>{});
}

// _____________________________________________________________________
//
// fit_3d — row-parallel fitting over (rows, cols) pixel grid
// _____________________________________________________________________
/**
 * @brief Fit all pixels in a 3D data cube (rows x cols x n_scan).
 *
 * @tparam Model  Model struct.
 * @tparam FCN    Chi2 functor type.
 *
 * @param model      Fit configuration shared by all pixels.
 * @param x          Scan points, shape `(n_scan)`.
 * @param y          Measured values, shape `(rows, cols, n_scan)`.
 * @param y_err      Uncertainties, same shape as y, or empty for unweighted fits.
 * @param par_out    Output parameters, shape `(rows, cols, npar)`.
 * @param err_out    Output parameter errors, shape `(rows, cols, npar)`, if used.
 * @param chi2_out   Output chi-squared / objective values, shape `(rows, cols)`.
 * @param n_threads  Number of threads used to split rows.
 *
 */
template <typename Model, typename FCN>
void fit_3d(const FitModel<Model>& model,
            NDView<double, 1> x,     // (n_scan)
            NDView<double, 3> y,     // (rows, cols, n_scan)
            NDView<double, 3> y_err, // (rows, cols, n_scan) or empty for unweighted fit
            NDView<double, 3> par_out,
            NDView<double, 3> err_out,
            NDView<double, 2> chi2_out,
            int n_threads)
{
    const std::size_t npar = Model::npar;

    // ──── checks ───────
    if (x.size() != y.shape(2))
        throw std::runtime_error("fit_3d: x.size() must match y.shape(2).");

    if (par_out.shape(0) != y.shape(0) || par_out.shape(1) != y.shape(1) || par_out.shape(2) != npar)
        throw std::runtime_error("par_out must have shape [rows, cols, npar].");

    if (chi2_out.shape(0) != y.shape(0) || chi2_out.shape(1) != y.shape(1))
        throw std::runtime_error("chi2_out must have shape [rows, cols].");
    
    const bool has_errors = (y_err.size() > 0);
    const bool want_par_errors = (err_out.size() > 0) && model.compute_errors();

    if (has_errors) {
        if (y.shape(0) != y_err.shape(0) || y.shape(1) != y_err.shape(1) || y.shape(2) != y_err.shape(2))
            throw std::runtime_error("fit_3d: y and y_err must have identical shape.");

        if (err_out.shape(0) != y.shape(0) || err_out.shape(1) != y.shape(1) || err_out.shape(2) != npar)
            throw std::runtime_error("err_out must have shape [rows, cols, npar].");
    }

    // ──── parallel dispatch ───────
    auto process = [&](ssize_t first_row, ssize_t last_row) {

        // one clone per thread 
        auto upar_local = model.upar(); 

        for (ssize_t row = first_row; row < last_row; row++) {
            for (ssize_t col = 0; col < y.shape(1); col++) {

                NDView<double, 1> values(&y(row, col, 0), {y.shape(2)});
                NDView<double, 1> errors = has_errors 
                        ? NDView<double, 1>(&y_err(row, col, 0), {y_err.shape(2)})
                        : NDView<double, 1>{};

                auto res = fit_pixel<Model, FCN>(model, upar_local, x, values, errors);
                
                for(std::size_t k = 0; k < npar; ++k) {
                    par_out(row, col, k) = res(k);
                }

                if (want_par_errors) {
                    for(std::size_t k = 0; k < npar; ++k){
                        err_out(row, col, k) = res(npar + k);
                    }
                    chi2_out(row, col) = res(2 * npar);
                } else {
                    chi2_out(row, col) = res(npar);
                }
            }
        }
    };

    auto tasks = split_task(0, static_cast<int>(y.shape(0)), n_threads);
    RunInParallel(process, tasks);
}

} // namespace aare