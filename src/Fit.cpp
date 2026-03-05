// SPDX-License-Identifier: MPL-2.0
#include "aare/Fit.hpp"
#include "aare/Chi2Gaussian.hpp"
#include "aare/Chi2GaussianGradient.hpp"
#include "aare/utils/par.hpp"
#include "aare/utils/task.hpp"
#include <lmcurve2.h>
#include <lmfit.hpp>
#include <thread>

#include <array>

#include "Minuit2/FunctionMinimum.h"
#include "Minuit2/MnMigrad.h"
#include "Minuit2/MnHesse.h"
#include "Minuit2/MnUserParameters.h"
#include "Minuit2/MnPrint.h"

namespace aare {

namespace func {

double gaus(const double x, const double *par) {
    return par[0] * exp(-pow(x - par[1], 2) / (2 * pow(par[2], 2)));
}

NDArray<double, 1> gaus(NDView<double, 1> x, NDView<double, 1> par) {
    NDArray<double, 1> y({x.shape(0)}, 0);
    for (ssize_t i = 0; i < x.size(); i++) {
        y(i) = gaus(x(i), par.data());
    }
    return y;
}

double pol1(const double x, const double *par) { return par[0] * x + par[1]; }

NDArray<double, 1> pol1(NDView<double, 1> x, NDView<double, 1> par) {
    NDArray<double, 1> y({x.shape()}, 0);
    for (ssize_t i = 0; i < x.size(); i++) {
        y(i) = pol1(x(i), par.data());
    }
    return y;
}

double scurve(const double x, const double *par) {
    return (par[0] + par[1] * x) +
           0.5 * (1 + erf((x - par[2]) / (sqrt(2) * par[3]))) *
               (par[4] + par[5] * (x - par[2]));
}

NDArray<double, 1> scurve(NDView<double, 1> x, NDView<double, 1> par) {
    NDArray<double, 1> y({x.shape()}, 0);
    for (ssize_t i = 0; i < x.size(); i++) {
        y(i) = scurve(x(i), par.data());
    }
    return y;
}

double scurve2(const double x, const double *par) {
    return (par[0] + par[1] * x) +
           0.5 * (1 - erf((x - par[2]) / (sqrt(2) * par[3]))) *
               (par[4] + par[5] * (x - par[2]));
}

NDArray<double, 1> scurve2(NDView<double, 1> x, NDView<double, 1> par) {
    NDArray<double, 1> y({x.shape()}, 0);
    for (ssize_t i = 0; i < x.size(); i++) {
        y(i) = scurve2(x(i), par.data());
    }
    return y;
}

} // namespace func

NDArray<double, 1> fit_gaus(NDView<double, 1> x, NDView<double, 1> y) {
    NDArray<double, 1> result = gaus_init_par(x, y);
    lm_status_struct status;

    lmcurve(result.size(), result.data(), x.size(), x.data(), y.data(),
            aare::func::gaus, &lm_control_double, &status);

    return result;
}

NDArray<double, 3> fit_gaus(NDView<double, 1> x, NDView<double, 3> y,
                            int n_threads) {
    NDArray<double, 3> result({y.shape(0), y.shape(1), 3}, 0);

    auto process = [&x, &y, &result](ssize_t first_row, ssize_t last_row) {
        for (ssize_t row = first_row; row < last_row; row++) {
            for (ssize_t col = 0; col < y.shape(1); col++) {
                NDView<double, 1> values(&y(row, col, 0), {y.shape(2)});
                auto res = fit_gaus(x, values);
                result(row, col, 0) = res(0);
                result(row, col, 1) = res(1);
                result(row, col, 2) = res(2);
            }
        }
    };

    auto tasks = split_task(0, y.shape(0), n_threads);
    RunInParallel(process, tasks);
    return result;
}

std::array<double, 3> gaus_init_par(const NDView<double, 1> x,
                                    const NDView<double, 1> y) {
    std::array<double, 3> start_par{0, 0, 0};
    auto e = std::max_element(y.begin(), y.end());
    auto idx = std::distance(y.begin(), e);

    start_par[0] = *e; // For amplitude we use the maximum value
    start_par[1] =
        x[idx]; // For the mean we use the x value of the maximum value

    // For sigma we estimate the fwhm and divide by 2.35
    // assuming equally spaced x values
    auto delta = x[1] - x[0];
    start_par[2] = std::count_if(y.begin(), y.end(),
                                 [e](double val) { return val > *e / 2; }) *
                   delta / 2.35;

    return start_par;
}

std::array<double, 2> pol1_init_par(const NDView<double, 1> x,
                                    const NDView<double, 1> y) {
    // Estimate the initial parameters for the fit
    std::array<double, 2> start_par{0, 0};

    auto y2 = std::max_element(y.begin(), y.end());
    auto x2 = x[std::distance(y.begin(), y2)];
    auto y1 = std::min_element(y.begin(), y.end());
    auto x1 = x[std::distance(y.begin(), y1)];

    start_par[0] =
        (*y2 - *y1) / (x2 - x1); // For amplitude we use the maximum value
    start_par[1] =
        *y1 - ((*y2 - *y1) / (x2 - x1)) *
                  x1; // For the mean we use the x value of the maximum value
    return start_par;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////
// MINUIT2
//////////////////////////////////////////////////////////////////////////////////////////////////////////

// -------------------------------------------
// Method 1: Minuit2 without analytic gradient
// -------------------------------------------
NDArray<double, 1> fit_gaus_minuit(NDView<double, 1> x,
                                NDView<double, 1> y,
                                NDView<double, 1> y_err) {

    auto start = gaus_init_par(x, y);

    // Guard against degenerate data (dead/noisy pixel)
    if (start[0] <= 0.0 || start[2] <= 0.0) {
        return NDArray<double, 1>({4}, 0.0); // [A, mu, sig, chi2]
    }

    // TODO: What if (y_err.size() > 0) but all values are zeros -> Should choose unweighted Chi2
    auto chi2 = (y_err.size() > 0)
        ? aare::func::Chi2Gaussian(x, y, y_err)
        : aare::func::Chi2Gaussian(x, y);


    ROOT::Minuit2::MnUserParameters upar;
    upar.Add("A",   start[0], start[0] * 0.1, 0.0, start[0] * 2.0);
    upar.Add("mu",  start[1], start[2] * 0.1,
             start[1] - 3.0 * start[2], start[1] + 3.0 * start[2]);
    upar.Add("sig", start[2], start[2] * 0.1, 0.0, start[2] * 5.0);

    ROOT::Minuit2::MnMigrad migrad(chi2, upar);
    ROOT::Minuit2::FunctionMinimum min = migrad();

    if (!min.IsValid())
        return NDArray<double, 1>({4}, 0.0);

    // Extract fitted parameters from the result
    NDArray<double, 1> result({4});
    result[0] = min.UserState().Value("A");
    result[1] = min.UserState().Value("mu");
    result[2] = min.UserState().Value("sig");
    result[3] = min.Fval();
    return result;
}

void fit_gaus_minuit_3d(NDView<double, 1> x, 
                        NDView<double, 3> y,
                        NDView<double, 3> y_err,
                        NDView<double, 3> par_out,
                        NDView<double, 2> chi2_out,
                        int n_threads )
{
    if (x.size() != y.shape(2)) {
        throw std::runtime_error(
            "fit_gaus_minuit_3d: x.size() must match y.shape(2).");
    }

    if (par_out.shape(0) != y.shape(0) || par_out.shape(1) != y.shape(1) || par_out.shape(2) != 3)
        throw std::runtime_error("par_out must have shape [rows, cols, 3].");

    if (chi2_out.shape(0) != y.shape(0) || chi2_out.shape(1) != y.shape(1))
        throw std::runtime_error("chi2_out must have shape [rows, cols].");

    const bool has_errors = (y_err.size() > 0);

    if (has_errors) {
        if (y.shape(0) != y_err.shape(0) ||
            y.shape(1) != y_err.shape(1) ||
            y.shape(2) != y_err.shape(2)) {
            throw std::runtime_error(
                "fit_gaus_minuit_3d: y_err must have the same shape as y.");
        }

        auto process = [&x, &y, &y_err, &par_out, &chi2_out](ssize_t first_row, ssize_t last_row) {
            for (ssize_t row = first_row; row < last_row; ++row) {
                for (ssize_t col = 0; col < y.shape(1); ++col) {
                    NDView<double, 1> values(&y(row, col, 0), {y.shape(2)});
                    NDView<double, 1> errors(&y_err(row, col, 0), {y_err.shape(2)});

                    auto res = fit_gaus_minuit(x, values, errors);
                    // res = [A, mu, sig, chi2]

                    par_out(row, col, 0) = res(0);
                    par_out(row, col, 1) = res(1);
                    par_out(row, col, 2) = res(2);
                    chi2_out(row, col) = res(3);
                }
            }
        };

        auto tasks = split_task(0, static_cast<int>(y.shape(0)), n_threads);
        RunInParallel(process, tasks);
    }
    else {
        auto process = [&x, &y, &par_out, &chi2_out](ssize_t first_row, ssize_t last_row) {
            for (ssize_t row = first_row; row < last_row; ++row) {
                for (ssize_t col = 0; col < y.shape(1); ++col) {
                    NDView<double, 1> values(&y(row, col, 0), {y.shape(2)});

                    auto res = fit_gaus_minuit(x, values);

                    par_out(row, col, 0) = res(0);
                    par_out(row, col, 1) = res(1);
                    par_out(row, col, 2) = res(2);
                    chi2_out(row, col) = res(3);
                }
            }
        };
    
        auto tasks = split_task(0, static_cast<int>(y.shape(0)), n_threads);
        RunInParallel(process, tasks);
    }
}

// Overload without an input `y_err`  
void fit_gaus_minuit_3d(NDView<double, 1> x, 
                        NDView<double, 3> y,
                        NDView<double, 3> par_out,
                        NDView<double, 2> chi2_out,
                        int n_threads) 
{
    fit_gaus_minuit_3d(x, y, NDView<double, 3>{}, par_out, chi2_out, n_threads);
}


// ------------------------------------------------------------------
// Method 2: Minuit2 with analytic gradient + Hesse errors (optional)
// ------------------------------------------------------------------
NDArray<double, 1> fit_gaus_minuit_grad(NDView<double, 1> x,
                                        NDView<double, 1> y,
                                        NDView<double, 1> y_err,
                                        bool compute_errors) {
    auto start = gaus_init_par(x, y);

    if (start[0] <= 0.0 || start[2] <= 0.0)
        return NDArray<double, 1>({compute_errors ? 7 : 4}, 0.0);

    auto chi2 = (y_err.size() > 0)
        ? aare::func::Chi2GaussianGradient(x, y, y_err)
        : aare::func::Chi2GaussianGradient(x, y);

    ROOT::Minuit2::MnUserParameters upar;
    // TODO: Optimize bounds
    upar.Add("A",   start[0], start[0] * 0.1, 0.0, start[0] * 2.0);
    upar.Add("mu",  start[1], start[2] * 0.1,
             start[1] - 3.0 * start[2], start[1] + 3.0 * start[2]);
    upar.Add("sig", start[2], start[2] * 0.1, 0.0, start[2] * 5.0);

    ROOT::Minuit2::MnMigrad migrad(chi2, upar);
    ROOT::Minuit2::FunctionMinimum min = migrad();

    if (!min.IsValid())
        return NDArray<double, 1>({compute_errors ? 7 : 4}, 0.0);

    if (compute_errors) {
        ROOT::Minuit2::MnHesse hesse;
        hesse(chi2, min);

        NDArray<double, 1> result({7});
        result[0] = min.UserState().Value("A");
        result[1] = min.UserState().Value("mu");
        result[2] = min.UserState().Value("sig");
        result[3] = min.UserState().Error("A");
        result[4] = min.UserState().Error("mu");
        result[5] = min.UserState().Error("sig");
        result[6] = min.Fval(); // chi2
        return result;
    }

    NDArray<double, 1> result({4});
    result[0] = min.UserState().Value("A");
    result[1] = min.UserState().Value("mu");
    result[2] = min.UserState().Value("sig");
    result[3] = min.Fval();
    return result;
}

void fit_gaus_minuit_grad_3d(NDView<double, 1> x, 
                            NDView<double, 3> y,
                            NDView<double, 3> y_err,
                            NDView<double, 3> par_out,
                            NDView<double, 3> err_out,
                            NDView<double, 2> chi2_out,
                            int n_threads) { // Always compute errors (see below (*) )

    if (x.size() != y.shape(2)) {
        throw std::runtime_error(
            "fit_gaus_minuit_grad_3d: x.size() must match y.shape(2).");
    }

    if (par_out.shape(0) != y.shape(0) || par_out.shape(1) != y.shape(1) || par_out.shape(2) != 3)
        throw std::runtime_error("par_out must have shape [rows, cols, 3].");

    if (err_out.shape(0) != y.shape(0) || err_out.shape(1) != y.shape(1) || err_out.shape(2) != 3)
        throw std::runtime_error("err_out must have shape [rows, cols, 3].");

    if (chi2_out.shape(0) != y.shape(0) || chi2_out.shape(1) != y.shape(1))
        throw std::runtime_error("chi2_out must have shape [rows, cols].");
    
    const bool has_errors = (y_err.size() > 0);

    if (has_errors) {
        if (y.shape(0) != y_err.shape(0) ||
        y.shape(1) != y_err.shape(1) ||
        y.shape(2) != y_err.shape(2)) {
        throw std::runtime_error(
            "fit_gaus_minuit_grad_3d: y and y_err must have identical shape.");
        }

        auto process = [&x, &y, &y_err, &par_out, &err_out, &chi2_out](ssize_t first_row, ssize_t last_row) {
            for (ssize_t row = first_row; row < last_row; row++) {
                for (ssize_t col = 0; col < y.shape(1); col++) {

                    NDView<double, 1> values(&y(row, col, 0), {y.shape(2)});
                    NDView<double, 1> errors(&y_err(row, col, 0), {y_err.shape(2)});

                    auto res = fit_gaus_minuit_grad(x, values, /*y_err = */ errors, /*compute_errors =*/ true ); // *Always compute errors
                    // res = [A, mu, sig, errA, errMu, errSig, chi2]

                    par_out(row, col, 0) = res(0);
                    par_out(row, col, 1) = res(1);
                    par_out(row, col, 2) = res(2);
                    err_out(row, col, 0) = res(3);
                    err_out(row, col, 1) = res(4);
                    err_out(row, col, 2) = res(5);
                    chi2_out(row, col) = res(6);
                }
            }
        };

        auto tasks = split_task(0, static_cast<int>(y.shape(0)), n_threads);
        RunInParallel(process, tasks);
    } else {
                auto process = [&x, &y, &par_out, &err_out, &chi2_out](ssize_t first_row, ssize_t last_row) {
            for (ssize_t row = first_row; row < last_row; row++) {
                for (ssize_t col = 0; col < y.shape(1); col++) {

                    NDView<double, 1> values(&y(row, col, 0), {y.shape(2)});

                    auto res = fit_gaus_minuit_grad(x, values, /*y_err = */ {}, /*compute_errors =*/ true ); // *Always compute errors

                    par_out(row, col, 0) = res(0);
                    par_out(row, col, 1) = res(1);
                    par_out(row, col, 2) = res(2);
                    err_out(row, col, 0) = res(3);
                    err_out(row, col, 1) = res(4);
                    err_out(row, col, 2) = res(5);
                    chi2_out(row, col) = res(6);
                }
            }
        };

        auto tasks = split_task(0, static_cast<int>(y.shape(0)), n_threads);
        RunInParallel(process, tasks);
    }

}

// Overload without input `y_err` 
void fit_gaus_minuit_grad_3d(NDView<double, 1> x, 
                            NDView<double, 3> y,
                            NDView<double, 3> par_out,
                            NDView<double, 3> err_out,
                            NDView<double, 2> chi2_out,
                            int n_threads) 
{
    fit_gaus_minuit_grad_3d(x, y, NDView<double, 3>{}, par_out, err_out, chi2_out, n_threads);
} 

//////////////////////////////////////////////////////////////////////////////////////////////////////////

void fit_gaus(NDView<double, 1> x, NDView<double, 1> y, NDView<double, 1> y_err,
              NDView<double, 1> par_out, NDView<double, 1> par_err_out,
              double &chi2) {

    // Check that we have the correct sizes
    if (y.size() != x.size() || y.size() != y_err.size() ||
        par_out.size() != 3 || par_err_out.size() != 3) {
        throw std::runtime_error("Data, x, data_err must have the same size "
                                 "and par_out, par_err_out must have size 3");
    }

    // /* Collection of output parameters for status info. */
    // typedef struct {
    //     double fnorm;  /* norm of the residue vector fvec. */
    //     int nfev;      /* actual number of iterations. */
    //     int outcome;   /* Status indicator. Nonnegative values are used as
    //     index
    //                     for the message text lm_infmsg, set in lmmin.c. */
    //     int userbreak; /* Set when function evaluation requests termination.
    //     */
    // } lm_status_struct;

    lm_status_struct status;
    par_out = gaus_init_par(x, y);
    std::array<double, 9> cov{0, 0, 0, 0, 0, 0, 0, 0, 0};

    // void lmcurve2( const int n_par, double *par, double *parerr, double
    // *covar, const int m_dat, const double *t, const double *y, const double
    // *dy, double (*f)( const double ti, const double *par ), const
    // lm_control_struct *control, lm_status_struct *status); n_par - Number of
    // free variables. Length of parameter vector par. par - Parameter vector.
    // On input, it must contain a reasonable guess. On output, it contains the
    // solution found to minimize ||r||. parerr - Parameter uncertainties
    // vector. Array of length n_par or NULL. On output, unless it or covar is
    // NULL, it contains the weighted parameter uncertainties for the found
    // parameters. covar - Covariance matrix. Array of length n_par * n_par or
    // NULL. On output, unless it is NULL, it contains the covariance matrix.
    // m_dat - Number of data points. Length of vectors t, y, dy. Must statisfy
    // n_par <= m_dat. t - Array of length m_dat. Contains the abcissae (time,
    // or "x") for which function f will be evaluated. y - Array of length
    // m_dat. Contains the ordinate values that shall be fitted. dy - Array of
    // length m_dat. Contains the standard deviations of the values y. f - A
    // user-supplied parametric function f(ti;par). control - Parameter
    // collection for tuning the fit procedure. In most cases, the default
    // &lm_control_double is adequate. If f is only computed with
    // single-precision accuracy, &lm_control_float should be used. Parameters
    // are explained in lmmin2(3). status - A record used to return information
    // about the minimization process: For details, see lmmin2(3).

    lmcurve2(par_out.size(), par_out.data(), par_err_out.data(), cov.data(),
             x.size(), x.data(), y.data(), y_err.data(), aare::func::gaus,
             &lm_control_double, &status);

    // Calculate chi2
    chi2 = 0;
    for (ssize_t i = 0; i < y.size(); i++) {
        chi2 +=
            std::pow((y(i) - func::gaus(x(i), par_out.data())) / y_err(i), 2);
    }
}

void fit_gaus(NDView<double, 1> x, NDView<double, 3> y, NDView<double, 3> y_err,
              NDView<double, 3> par_out, NDView<double, 3> par_err_out,
              NDView<double, 2> chi2_out,

              int n_threads) {

    auto process = [&](ssize_t first_row, ssize_t last_row) {
        for (ssize_t row = first_row; row < last_row; row++) {
            for (ssize_t col = 0; col < y.shape(1); col++) {
                NDView<double, 1> y_view(&y(row, col, 0), {y.shape(2)});
                NDView<double, 1> y_err_view(&y_err(row, col, 0),
                                             {y_err.shape(2)});
                NDView<double, 1> par_out_view(&par_out(row, col, 0),
                                               {par_out.shape(2)});
                NDView<double, 1> par_err_out_view(&par_err_out(row, col, 0),
                                                   {par_err_out.shape(2)});

                fit_gaus(x, y_view, y_err_view, par_out_view, par_err_out_view,
                         chi2_out(row, col));
            }
        }
    };

    auto tasks = split_task(0, y.shape(0), n_threads);
    RunInParallel(process, tasks);
}

void fit_pol1(NDView<double, 1> x, NDView<double, 1> y, NDView<double, 1> y_err,
              NDView<double, 1> par_out, NDView<double, 1> par_err_out,
              double &chi2) {

    // Check that we have the correct sizes
    if (y.size() != x.size() || y.size() != y_err.size() ||
        par_out.size() != 2 || par_err_out.size() != 2) {
        throw std::runtime_error("Data, x, data_err must have the same size "
                                 "and par_out, par_err_out must have size 2");
    }

    lm_status_struct status;
    par_out = pol1_init_par(x, y);
    std::array<double, 4> cov{0, 0, 0, 0};

    lmcurve2(par_out.size(), par_out.data(), par_err_out.data(), cov.data(),
             x.size(), x.data(), y.data(), y_err.data(), aare::func::pol1,
             &lm_control_double, &status);

    // Calculate chi2
    chi2 = 0;
    for (ssize_t i = 0; i < y.size(); i++) {
        chi2 +=
            std::pow((y(i) - func::pol1(x(i), par_out.data())) / y_err(i), 2);
    }
}

void fit_pol1(NDView<double, 1> x, NDView<double, 3> y, NDView<double, 3> y_err,
              NDView<double, 3> par_out, NDView<double, 3> par_err_out,
              NDView<double, 2> chi2_out, int n_threads) {

    auto process = [&](ssize_t first_row, ssize_t last_row) {
        for (ssize_t row = first_row; row < last_row; row++) {
            for (ssize_t col = 0; col < y.shape(1); col++) {
                NDView<double, 1> y_view(&y(row, col, 0), {y.shape(2)});
                NDView<double, 1> y_err_view(&y_err(row, col, 0),
                                             {y_err.shape(2)});
                NDView<double, 1> par_out_view(&par_out(row, col, 0),
                                               {par_out.shape(2)});
                NDView<double, 1> par_err_out_view(&par_err_out(row, col, 0),
                                                   {par_err_out.shape(2)});

                fit_pol1(x, y_view, y_err_view, par_out_view, par_err_out_view,
                         chi2_out(row, col));
            }
        }
    };

    auto tasks = split_task(0, y.shape(0), n_threads);
    RunInParallel(process, tasks);
}

NDArray<double, 1> fit_pol1(NDView<double, 1> x, NDView<double, 1> y) {
    // // Check that we have the correct sizes
    // if (y.size() != x.size() || y.size() != y_err.size() ||
    // par_out.size() != 2 || par_err_out.size() != 2) {
    // throw std::runtime_error("Data, x, data_err must have the same size "
    //                      "and par_out, par_err_out must have size 2");
    // }
    NDArray<double, 1> par = pol1_init_par(x, y);

    lm_status_struct status;
    lmcurve(par.size(), par.data(), x.size(), x.data(), y.data(),
            aare::func::pol1, &lm_control_double, &status);

    return par;
}

NDArray<double, 3> fit_pol1(NDView<double, 1> x, NDView<double, 3> y,
                            int n_threads) {
    NDArray<double, 3> result({y.shape(0), y.shape(1), 2}, 0);

    auto process = [&](ssize_t first_row, ssize_t last_row) {
        for (ssize_t row = first_row; row < last_row; row++) {
            for (ssize_t col = 0; col < y.shape(1); col++) {
                NDView<double, 1> values(&y(row, col, 0), {y.shape(2)});
                auto res = fit_pol1(x, values);
                result(row, col, 0) = res(0);
                result(row, col, 1) = res(1);
            }
        }
    };

    auto tasks = split_task(0, y.shape(0), n_threads);

    RunInParallel(process, tasks);
    return result;
}

// ~~ S-CURVES ~~

// SCURVE --
std::array<double, 6> scurve_init_par(const NDView<double, 1> x,
                                      const NDView<double, 1> y) {
    // Estimate the initial parameters for the fit
    std::array<double, 6> start_par{0, 0, 0, 0, 0, 0};

    auto ymax = std::max_element(y.begin(), y.end());
    auto ymin = std::min_element(y.begin(), y.end());
    start_par[4] = *ymin + (*ymax - *ymin) / 2;

    // Find the first x where the corresponding y value is above the threshold
    // (start_par[4])
    for (ssize_t i = 0; i < y.size(); ++i) {
        if (y[i] >= start_par[4]) {
            start_par[2] = x[i];
            break; // Exit the loop after finding the first valid x
        }
    }

    start_par[3] = 2 * sqrt(start_par[2]);
    start_par[0] = 100;
    start_par[1] = 0.25;
    start_par[5] = 1;
    return start_par;
}

// - No error
NDArray<double, 1> fit_scurve(NDView<double, 1> x, NDView<double, 1> y) {
    NDArray<double, 1> result = scurve_init_par(x, y);
    lm_status_struct status;

    lmcurve(result.size(), result.data(), x.size(), x.data(), y.data(),
            aare::func::scurve, &lm_control_double, &status);

    return result;
}

NDArray<double, 3> fit_scurve(NDView<double, 1> x, NDView<double, 3> y,
                              int n_threads) {
    NDArray<double, 3> result({y.shape(0), y.shape(1), 6}, 0);

    auto process = [&x, &y, &result](ssize_t first_row, ssize_t last_row) {
        for (ssize_t row = first_row; row < last_row; row++) {
            for (ssize_t col = 0; col < y.shape(1); col++) {
                NDView<double, 1> values(&y(row, col, 0), {y.shape(2)});
                auto res = fit_scurve(x, values);
                result(row, col, 0) = res(0);
                result(row, col, 1) = res(1);
                result(row, col, 2) = res(2);
                result(row, col, 3) = res(3);
                result(row, col, 4) = res(4);
                result(row, col, 5) = res(5);
            }
        }
    };

    auto tasks = split_task(0, y.shape(0), n_threads);
    RunInParallel(process, tasks);
    return result;
}

// - Error
void fit_scurve(NDView<double, 1> x, NDView<double, 1> y,
                NDView<double, 1> y_err, NDView<double, 1> par_out,
                NDView<double, 1> par_err_out, double &chi2) {

    // Check that we have the correct sizes
    if (y.size() != x.size() || y.size() != y_err.size() ||
        par_out.size() != 6 || par_err_out.size() != 6) {
        throw std::runtime_error("Data, x, data_err must have the same size "
                                 "and par_out, par_err_out must have size 6");
    }

    lm_status_struct status;
    par_out = scurve_init_par(x, y);
    std::array<double, 36> cov = {0}; // size 6x6
    // std::array<double, 4> cov{0, 0, 0, 0};

    lmcurve2(par_out.size(), par_out.data(), par_err_out.data(), cov.data(),
             x.size(), x.data(), y.data(), y_err.data(), aare::func::scurve,
             &lm_control_double, &status);

    // Calculate chi2
    chi2 = 0;
    for (ssize_t i = 0; i < y.size(); i++) {
        chi2 +=
            std::pow((y(i) - func::pol1(x(i), par_out.data())) / y_err(i), 2);
    }
}

void fit_scurve(NDView<double, 1> x, NDView<double, 3> y,
                NDView<double, 3> y_err, NDView<double, 3> par_out,
                NDView<double, 3> par_err_out, NDView<double, 2> chi2_out,
                int n_threads) {

    auto process = [&](ssize_t first_row, ssize_t last_row) {
        for (ssize_t row = first_row; row < last_row; row++) {
            for (ssize_t col = 0; col < y.shape(1); col++) {
                NDView<double, 1> y_view(&y(row, col, 0), {y.shape(2)});
                NDView<double, 1> y_err_view(&y_err(row, col, 0),
                                             {y_err.shape(2)});
                NDView<double, 1> par_out_view(&par_out(row, col, 0),
                                               {par_out.shape(2)});
                NDView<double, 1> par_err_out_view(&par_err_out(row, col, 0),
                                                   {par_err_out.shape(2)});

                fit_scurve(x, y_view, y_err_view, par_out_view,
                           par_err_out_view, chi2_out(row, col));
            }
        }
    };

    auto tasks = split_task(0, y.shape(0), n_threads);
    RunInParallel(process, tasks);
}

// SCURVE2 ---

std::array<double, 6> scurve2_init_par(const NDView<double, 1> x,
                                       const NDView<double, 1> y) {
    // Estimate the initial parameters for the fit
    std::array<double, 6> start_par{0, 0, 0, 0, 0, 0};

    auto ymax = std::max_element(y.begin(), y.end());
    auto ymin = std::min_element(y.begin(), y.end());
    start_par[4] = *ymin + (*ymax - *ymin) / 2;

    // Find the first x where the corresponding y value is above the threshold
    // (start_par[4])
    for (ssize_t i = 0; i < y.size(); ++i) {
        if (y[i] <= start_par[4]) {
            start_par[2] = x[i];
            break; // Exit the loop after finding the first valid x
        }
    }

    start_par[3] = 2 * sqrt(start_par[2]);
    start_par[0] = 100;
    start_par[1] = 0.25;
    start_par[5] = -1;
    return start_par;
}

// - No error
NDArray<double, 1> fit_scurve2(NDView<double, 1> x, NDView<double, 1> y) {
    NDArray<double, 1> result = scurve2_init_par(x, y);
    lm_status_struct status;

    lmcurve(result.size(), result.data(), x.size(), x.data(), y.data(),
            aare::func::scurve2, &lm_control_double, &status);

    return result;
}

NDArray<double, 3> fit_scurve2(NDView<double, 1> x, NDView<double, 3> y,
                               int n_threads) {
    NDArray<double, 3> result({y.shape(0), y.shape(1), 6}, 0);

    auto process = [&x, &y, &result](ssize_t first_row, ssize_t last_row) {
        for (ssize_t row = first_row; row < last_row; row++) {
            for (ssize_t col = 0; col < y.shape(1); col++) {
                NDView<double, 1> values(&y(row, col, 0), {y.shape(2)});
                auto res = fit_scurve2(x, values);
                result(row, col, 0) = res(0);
                result(row, col, 1) = res(1);
                result(row, col, 2) = res(2);
                result(row, col, 3) = res(3);
                result(row, col, 4) = res(4);
                result(row, col, 5) = res(5);
            }
        }
    };

    auto tasks = split_task(0, y.shape(0), n_threads);
    RunInParallel(process, tasks);
    return result;
}

// - Error
void fit_scurve2(NDView<double, 1> x, NDView<double, 1> y,
                 NDView<double, 1> y_err, NDView<double, 1> par_out,
                 NDView<double, 1> par_err_out, double &chi2) {

    // Check that we have the correct sizes
    if (y.size() != x.size() || y.size() != y_err.size() ||
        par_out.size() != 6 || par_err_out.size() != 6) {
        throw std::runtime_error("Data, x, data_err must have the same size "
                                 "and par_out, par_err_out must have size 6");
    }

    lm_status_struct status;
    par_out = scurve2_init_par(x, y);
    std::array<double, 36> cov = {0}; // size 6x6
    // std::array<double, 4> cov{0, 0, 0, 0};

    lmcurve2(par_out.size(), par_out.data(), par_err_out.data(), cov.data(),
             x.size(), x.data(), y.data(), y_err.data(), aare::func::scurve2,
             &lm_control_double, &status);

    // Calculate chi2
    chi2 = 0;
    for (ssize_t i = 0; i < y.size(); i++) {
        chi2 +=
            std::pow((y(i) - func::pol1(x(i), par_out.data())) / y_err(i), 2);
    }
}

void fit_scurve2(NDView<double, 1> x, NDView<double, 3> y,
                 NDView<double, 3> y_err, NDView<double, 3> par_out,
                 NDView<double, 3> par_err_out, NDView<double, 2> chi2_out,
                 int n_threads) {

    auto process = [&](ssize_t first_row, ssize_t last_row) {
        for (ssize_t row = first_row; row < last_row; row++) {
            for (ssize_t col = 0; col < y.shape(1); col++) {
                NDView<double, 1> y_view(&y(row, col, 0), {y.shape(2)});
                NDView<double, 1> y_err_view(&y_err(row, col, 0),
                                             {y_err.shape(2)});
                NDView<double, 1> par_out_view(&par_out(row, col, 0),
                                               {par_out.shape(2)});
                NDView<double, 1> par_err_out_view(&par_err_out(row, col, 0),
                                                   {par_err_out.shape(2)});

                fit_scurve2(x, y_view, y_err_view, par_out_view,
                            par_err_out_view, chi2_out(row, col));
            }
        }
    };

    auto tasks = split_task(0, y.shape(0), n_threads);
    RunInParallel(process, tasks);
}

} // namespace aare