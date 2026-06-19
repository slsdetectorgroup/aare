// SPDX-License-Identifier: MPL-2.0
#include "aare/Fit.hpp"
#include "Chi2.hpp"
#include "FitModelImpl.hpp"
#include "Minuit2/FunctionMinimum.h"
#include "Minuit2/MnHesse.h"
#include "Minuit2/MnMigrad.h"
#include "Minuit2/MnPrint.h"
#include "Minuit2/MnUserParameters.h"
#include "aare/Models.hpp"
#include "aare/utils/par.hpp"
#include "aare/utils/task.hpp"
#include <array>
#include <cmath>
#include <lmcurve2.h>
#include <lmfit.hpp>
#include <memory>
#include <stdexcept>
#include <thread>
#include <type_traits>

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

// ============================================================================
// FitModel<Model> — method definitions
// (constructor, destructor, copy, and all methods that touch Minuit2 state)
// ============================================================================

template <typename Model>
FitModel<Model>::FitModel(unsigned int strategy, unsigned int max_calls,
                          double tolerance, bool compute_errors)
    : impl_(std::make_unique<FitModelImpl>(strategy)), max_calls_(max_calls),
      tolerance_(tolerance), compute_errors_(compute_errors) {
    for (std::size_t i = 0; i < npar; ++i) {
        const auto pi = Model::param_info[i];
        const bool has_lo = std::isfinite(pi.default_lo);
        const bool has_hi = std::isfinite(pi.default_hi);
        if (has_lo && has_hi) {
            impl_->upar.Add(pi.name, 0.0, 1.0, pi.default_lo, pi.default_hi);
        } else if (has_lo) {
            impl_->upar.Add(pi.name, 0.0, 1.0, pi.default_lo, 1e6);
        } else {
            impl_->upar.Add(pi.name, 0.0, 1.0);
        }
    }
}

template <typename Model> FitModel<Model>::~FitModel() = default;

template <typename Model>
FitModel<Model>::FitModel(const FitModel &other)
    : impl_(std::make_unique<FitModelImpl>(*other.impl_)),
      max_calls_(other.max_calls_), tolerance_(other.tolerance_),
      compute_errors_(other.compute_errors_), user_fixed_(other.user_fixed_),
      user_start_(other.user_start_) {}

template <typename Model>
FitModel<Model> &FitModel<Model>::operator=(const FitModel &other) {
    if (this != &other) {
        impl_ = std::make_unique<FitModelImpl>(*other.impl_);
        max_calls_ = other.max_calls_;
        tolerance_ = other.tolerance_;
        compute_errors_ = other.compute_errors_;
        user_fixed_ = other.user_fixed_;
        user_start_ = other.user_start_;
    }
    return *this;
}

template <typename Model>
unsigned int FitModel<Model>::checked_index(const std::string &name) const {
    for (std::size_t i = 0; i < npar; ++i) {
        if (impl_->upar.Name(i) == name)
            return static_cast<unsigned int>(i);
    }
    throw std::runtime_error("FitModel: unknown parameter name '" + name + "'");
}

template <typename Model>
void FitModel<Model>::SetParLimits(unsigned int idx, double lo, double hi) {
    impl_->upar.SetLimits(idx, lo, hi);
}

template <typename Model>
void FitModel<Model>::FixParameter(unsigned int idx, double val) {
    SetParameter(idx, val);
    impl_->upar.Fix(idx);
    user_fixed_[idx] = true;
}

template <typename Model>
void FitModel<Model>::ReleaseParameter(unsigned int idx) {
    impl_->upar.Release(idx);
    user_fixed_[idx] = false;
}

template <typename Model>
void FitModel<Model>::ReleaseParameter(const std::string &name) {
    ReleaseParameter(checked_index(name));
}

template <typename Model>
void FitModel<Model>::SetParameter(unsigned int idx, double val) {
    impl_->upar.SetValue(idx, val);
    user_start_[idx] = true;
}

template <typename Model>
void FitModel<Model>::SetParameter(const std::string &name, double val) {
    SetParameter(checked_index(name), val);
}

template <typename Model>
void FitModel<Model>::FixParameter(const std::string &name, double val) {
    FixParameter(checked_index(name), val);
}

template <typename Model>
void FitModel<Model>::SetParLimits(const std::string &name, double lo,
                                   double hi) {
    SetParLimits(checked_index(name), lo, hi);
}

template <typename Model>
std::string FitModel<Model>::GetParName(unsigned int idx) const {
    return impl_->upar.GetName(idx);
}

template <typename Model>
std::vector<std::string> FitModel<Model>::GetParNames() const {
    std::vector<std::string> names;
    for (std::size_t i = 0; i < npar; ++i)
        names.push_back(GetParName(i));
    return names;
}

// ============================================================================
// fit_pixel / fit_3d — Minuit2 template implementations
// ============================================================================

template <typename Model>
NDArray<double, 1> fit_pixel(const FitModel<Model> &model, NDView<double, 1> x,
                             NDView<double, 1> y, NDView<double, 1> y_err) {
    using FCN = func::Chi2Model1DGrad<Model>;
    constexpr std::size_t npar = Model::npar;
    const bool want_errors = model.compute_errors();
    const ssize_t result_size = want_errors ? (2 * npar + 1) : (npar + 1);

    auto start = Model::estimate_par(x, y);

    if (!Model::is_valid(std::vector<double>(start.begin(), start.end()))) {
        return NDArray<double, 1>({result_size}, 0.0);
    }

    double x_range, y_range, slope_scale;
    model::compute_ranges(x, y, x_range, y_range, slope_scale);

    std::array<double, npar> steps{};
    Model::compute_steps(start, x_range, y_range, slope_scale, steps);

    // thread-local copy of starting parameters
    auto upar_local = model.impl()->upar;

    for (std::size_t i = 0; i < npar; ++i) {
        if (model.is_user_fixed(i))
            continue;
        if (!model.is_user_start(i))
            upar_local.SetValue(i, start[i]);
        upar_local.SetError(i, steps[i]);
    }

    auto chi2 = (y_err.size() > 0) ? FCN(x, y, y_err) : FCN(x, y);

    ROOT::Minuit2::MnMigrad migrad(chi2, upar_local, model.impl()->strategy);
    ROOT::Minuit2::FunctionMinimum min =
        migrad(model.max_calls(), model.tolerance());

    if (!min.IsValid())
        return NDArray<double, 1>({result_size}, 0.0);

    if (want_errors) {
        ROOT::Minuit2::MnHesse hesse;
        hesse(chi2, min);

        const auto &values = min.UserState().Params();
        const auto &errors = min.UserState().Errors();

        NDArray<double, 1> result({result_size});
        for (std::size_t k = 0; k < npar; ++k) {
            result[k] = values[k];
            result[npar + k] = errors[k];
        }
        result[2 * npar] = min.Fval();
        return result;
    }

    const auto &values = min.UserState().Params();
    NDArray<double, 1> result({result_size});
    for (std::size_t k = 0; k < npar; ++k)
        result[k] = values[k];
    result[npar] = min.Fval();
    return result;
}

template <typename Model>
NDArray<double, 1> fit_pixel(const FitModel<Model> &model, NDView<double, 1> x,
                             NDView<double, 1> y) {
    return fit_pixel(model, x, y, NDView<double, 1>{});
}

template <typename Model>
void fit_3d(const FitModel<Model> &model, NDView<double, 1> x,
            NDView<double, 3> y, NDView<double, 3> y_err,
            NDView<double, 3> par_out, NDView<double, 3> err_out,
            NDView<double, 2> chi2_out, int n_threads) {
    const std::size_t npar = Model::npar;

    if (x.size() != y.shape(2))
        throw std::runtime_error("fit_3d: x.size() must match y.shape(2).");

    if (par_out.shape(0) != y.shape(0) || par_out.shape(1) != y.shape(1) ||
        par_out.shape(2) != npar)
        throw std::runtime_error("par_out must have shape [rows, cols, npar].");

    if (chi2_out.shape(0) != y.shape(0) || chi2_out.shape(1) != y.shape(1))
        throw std::runtime_error("chi2_out must have shape [rows, cols].");

    const bool has_errors = (y_err.size() > 0);
    const bool want_par_errors = (err_out.size() > 0) && model.compute_errors();

    if (has_errors) {
        if (y.shape(0) != y_err.shape(0) || y.shape(1) != y_err.shape(1) ||
            y.shape(2) != y_err.shape(2))
            throw std::runtime_error(
                "fit_3d: y and y_err must have identical shape.");

        if (err_out.shape(0) != y.shape(0) || err_out.shape(1) != y.shape(1) ||
            err_out.shape(2) != npar)
            throw std::runtime_error(
                "err_out must have shape [rows, cols, npar].");
    }

    auto process = [&](ssize_t first_row, ssize_t last_row) {
        for (ssize_t row = first_row; row < last_row; row++) {
            for (ssize_t col = 0; col < y.shape(1); col++) {

                NDView<double, 1> values(&y(row, col, 0), {y.shape(2)});
                NDView<double, 1> errors =
                    has_errors ? NDView<double, 1>(&y_err(row, col, 0),
                                                   {y_err.shape(2)})
                               : NDView<double, 1>{};

                auto res = fit_pixel(model, x, values, errors);

                for (std::size_t k = 0; k < npar; ++k)
                    par_out(row, col, k) = res(k);

                if (want_par_errors) {
                    for (std::size_t k = 0; k < npar; ++k)
                        err_out(row, col, k) = res(npar + k);
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

// ============================================================================
// Explicit instantiations for all supported model types
// ============================================================================

// NOLINTBEGIN
#define AARE_INSTANTIATE_FIT(Model)                                            \
    template class FitModel<model::Model>;                                     \
    template NDArray<double, 1> fit_pixel<model::Model>(                       \
        const FitModel<model::Model> &, NDView<double, 1>, NDView<double, 1>,  \
        NDView<double, 1>);                                                    \
    template NDArray<double, 1> fit_pixel<model::Model>(                       \
        const FitModel<model::Model> &, NDView<double, 1>, NDView<double, 1>); \
    template void fit_3d<model::Model>(                                        \
        const FitModel<model::Model> &, NDView<double, 1>, NDView<double, 3>,  \
        NDView<double, 3>, NDView<double, 3>, NDView<double, 3>,               \
        NDView<double, 2>, int);

AARE_INSTANTIATE_FIT(Gaussian)
AARE_INSTANTIATE_FIT(GaussianErfcPlateau)
AARE_INSTANTIATE_FIT(GaussianChargeSharing)
AARE_INSTANTIATE_FIT(GaussianChargeSharingKb)
AARE_INSTANTIATE_FIT(Pol1)
AARE_INSTANTIATE_FIT(Pol2)
AARE_INSTANTIATE_FIT(RisingScurve)
AARE_INSTANTIATE_FIT(FallingScurve)

#undef AARE_INSTANTIATE_FIT
// NOLINTEND

} // namespace aare