// SPDX-License-Identifier: MPL-2.0
#include "aare/Fit.hpp"
#include "aare/Chi2.hpp"
#include "aare/Models.hpp"
#include "aare/utils/par.hpp"
#include "aare/utils/task.hpp"
#include <lmcurve2.h>
#include <lmfit.hpp>
#include <thread>
#include <type_traits>
#include <array>

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
    NDArray<double, 1> result = model::Gaussian::estimate_par(x, y);
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
    par_out = model::Gaussian::estimate_par(x, y);
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
    par_out = model::Pol1::estimate_par(x, y);
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
    NDArray<double, 1> par = model::Pol1::estimate_par(x, y);

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


// - No error
NDArray<double, 1> fit_scurve(NDView<double, 1> x, NDView<double, 1> y) {
    NDArray<double, 1> result = model::RisingScurve::estimate_par(x, y);
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
    par_out = model::RisingScurve::estimate_par(x, y);
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



// - No error
NDArray<double, 1> fit_scurve2(NDView<double, 1> x, NDView<double, 1> y) {
    NDArray<double, 1> result = model::FallingScurve::estimate_par(x, y);
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
    par_out = model::FallingScurve::estimate_par(x, y);
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