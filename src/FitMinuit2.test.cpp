// SPDX-License-Identifier: MPL-2.0
// Cross-check of the built-in Levenberg-Marquardt solver against the optional
// Minuit2 backend. Compiled only with -DAARE_MINUIT2=ON.
#include "aare/FitMinuit2.hpp"
#include "aare/Fit.hpp"
#include "aare/FitModel.hpp"
#include "aare/Models.hpp"

#include <algorithm>
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <cmath>
#include <functional>
#include <string>
#include <vector>

using aare::NDArray;
using aare::NDView;

namespace {

struct Data {
    NDArray<double, 1> x;
    NDArray<double, 1> y;
    NDArray<double, 1> y_err;
};

// Noise-free Gaussian, A = 120, mu = 0.8, sigma = 1.3, unit errors.
Data gaussian_data(bool add_noise) {
    constexpr ssize_t n = 61;
    Data d{NDArray<double, 1>({n}), NDArray<double, 1>({n}),
           NDArray<double, 1>({n}, 1.0)};
    for (ssize_t i = 0; i < n; ++i) {
        d.x(i) = -6.0 + 0.2 * static_cast<double>(i);
        const double z = (d.x(i) - 0.8) / 1.3;
        d.y(i) = 120.0 * std::exp(-0.5 * z * z);
        if (add_noise) // deterministic pseudo-noise
            d.y(i) += 3.0 * std::sin(7.3 * static_cast<double>(i));
    }
    return d;
}

template <typename Model>
Data scurve_data(const std::vector<double> &par, bool add_noise) {
    constexpr ssize_t n = 100;
    Data d{NDArray<double, 1>({n}), NDArray<double, 1>({n}),
           NDArray<double, 1>({n}, 1.0)};
    for (ssize_t i = 0; i < n; ++i) {
        d.x(i) = 100.0 * static_cast<double>(i) / static_cast<double>(n - 1);
        d.y(i) = Model::eval(d.x(i), par);
        if (add_noise) {
            d.y(i) += 30.0 * std::sin(7.3 * static_cast<double>(i));
            d.y_err(i) = 30.0;
        }
    }
    return d;
}

// Run the same configuration through both backends. The fits must agree on
// parameters and chi2 (relative tolerance tol), unless the built-in solver
// found a clearly lower chi2. Errors are compared for free parameters that
// do not end on a limit, with tolerance err_tol: Gauss-Newton errors match
// Hesse at a good minimum but differ when a parameter is forced to a wrong
// value, so misfit cases pass a looser err_tol.
template <typename Model, typename Setup>
void compare_backends(const std::string &name, Data d, bool weighted,
                      bool errors, unsigned max_calls, double tol, Setup setup,
                      double err_tol = 0.0) {
    if (err_tol == 0.0)
        err_tol = 10.0 * tol;
    constexpr std::size_t npar = Model::npar;
    aare::FitModel<Model> model(0, max_calls, 0.5, errors);
    setup(model);
    NDView<double, 1> ev = weighted ? d.y_err.view() : NDView<double, 1>{};
    const auto lm = weighted
                        ? aare::fit_pixel(model, d.x.view(), d.y.view(), ev)
                        : aare::fit_pixel(model, d.x.view(), d.y.view());
    const auto mn =
        weighted ? aare::minuit2::fit_pixel(model, d.x.view(), d.y.view(), ev)
                 : aare::minuit2::fit_pixel(model, d.x.view(), d.y.view());
    INFO(name);
    REQUIRE(lm.size() == mn.size());

    const auto ichi = static_cast<ssize_t>(errors ? 2 * npar : npar);
    const double chi_lm = lm(ichi);
    const double chi_mn = mn(ichi);
    if (chi_lm > 0.0 && chi_mn > 0.0 &&
        chi_mn - chi_lm > tol * std::max(chi_mn, 1.0)) {
        WARN(name + ": built-in solver found a lower chi2 (" +
             std::to_string(chi_lm) + " vs " + std::to_string(chi_mn) + ")");
        return;
    }
    double scale = 0.0;
    for (std::size_t k = 0; k < npar; ++k)
        scale = std::max(scale, std::abs(mn(static_cast<ssize_t>(k))));
    for (std::size_t k = 0; k < npar; ++k) {
        const auto i = static_cast<ssize_t>(k);
        const double denom = std::max({std::abs(mn(i)), 1e-2 * scale, 1e-12});
        INFO("parameter " << model.GetParName(static_cast<unsigned int>(k)));
        CHECK(std::abs(lm(i) - mn(i)) / denom <= tol);
    }
    CHECK(std::abs(chi_lm - chi_mn) / std::max(std::abs(chi_mn), 1.0) <= tol);
    if (!errors)
        return;
    for (std::size_t k = 0; k < npar; ++k) {
        const auto idx = static_cast<unsigned int>(k);
        const auto i = static_cast<ssize_t>(k);
        const double v = mn(i);
        const double tolb = 1e-6 * std::max(1.0, std::abs(v));
        const bool on_limit = std::abs(v - model.lower_limit(idx)) <= tolb ||
                              std::abs(v - model.upper_limit(idx)) <= tolb;
        if (model.is_user_fixed(idx) || on_limit)
            continue;
        const auto ie = static_cast<ssize_t>(npar + k);
        INFO("error of " << model.GetParName(idx));
        CHECK(std::abs(lm(ie) - mn(ie)) / std::max(std::abs(mn(ie)), 1e-12) <=
              err_tol);
    }
}

} // namespace

TEST_CASE("Built-in solver matches the Minuit2 backend on Gaussian fits",
          "[fit][minuit2]") {
    using aare::model::Gaussian;
    const auto none = [](auto &) {};
    compare_backends<Gaussian>("exact, unweighted", gaussian_data(false), false,
                               false, 100, 1e-4, none);
    compare_backends<Gaussian>("exact, weighted with errors",
                               gaussian_data(false), true, true, 100, 1e-4,
                               none);
    compare_backends<Gaussian>(
        "sigma fixed at a wrong value", gaussian_data(false), true, true, 100,
        1e-3, [](auto &m) { m.FixParameter("sigma", 1.5); }, 0.2);
    compare_backends<Gaussian>("sigma fixed at the true value",
                               gaussian_data(false), true, true, 100, 1e-3,
                               [](auto &m) { m.FixParameter("sigma", 1.3); });
    compare_backends<Gaussian>(
        "active upper limit on mu", gaussian_data(false), false, false, 100,
        1e-3, [](auto &m) { m.SetParLimits("mu", -5.0, 0.5); });
    compare_backends<Gaussian>(
        "inactive limits", gaussian_data(false), false, false, 100, 1e-4,
        [](auto &m) { m.SetParLimits("mu", -5.0, 5.0); });
    compare_backends<Gaussian>(
        "active upper limit on sigma", gaussian_data(false), true, true, 100,
        1e-3, [](auto &m) { m.SetParLimits("sigma", 0.5, 1.0); }, 0.2);
    compare_backends<Gaussian>("user start value", gaussian_data(false), false,
                               false, 100, 1e-4,
                               [](auto &m) { m.SetParameter("mu", 0.0); });
    compare_backends<Gaussian>(
        "fixed amplitude and active limit on mu", gaussian_data(false), true,
        true, 100, 1e-3,
        [](auto &m) {
            m.FixParameter("A", 100.0);
            m.SetParLimits("mu", 0.6, 0.7);
        },
        0.2);
    compare_backends<Gaussian>("max_calls exhausted", gaussian_data(false),
                               false, false, 3, 1e-9, none);
    compare_backends<Gaussian>("noisy, weighted with errors",
                               gaussian_data(true), true, true, 500, 1e-3,
                               none);
    compare_backends<Gaussian>(
        "noisy, fixed mu and active limit on sigma", gaussian_data(true), true,
        true, 500, 1e-3,
        [](auto &m) {
            m.FixParameter("mu", 0.8);
            m.SetParLimits("sigma", 0.5, 1.1);
        },
        0.2);
}

TEST_CASE("Built-in solver matches the Minuit2 backend on S-curve fits",
          "[fit][minuit2]") {
    using aare::model::RisingScurve;
    const std::vector<double> truth{10.0, 0.1, 50.0, 3.0, 1000.0, 2.0};
    const auto none = [](auto &) {};
    compare_backends<RisingScurve>("noisy, weighted with errors",
                                   scurve_data<RisingScurve>(truth, true), true,
                                   true, 500, 2e-2, none);
    compare_backends<RisingScurve>("noisy, fixed slope and limited width",
                                   scurve_data<RisingScurve>(truth, true), true,
                                   true, 500, 1e-2, [](auto &m) {
                                       m.FixParameter("p1", 0.1);
                                       m.SetParLimits("sigma", 1.0, 4.0);
                                   });
    compare_backends<RisingScurve>(
        "noisy, active limit on the threshold",
        scurve_data<RisingScurve>(truth, true), false, false, 500, 1e-2,
        [](auto &m) { m.SetParLimits("mu", 40.0, 48.0); });
    compare_backends<RisingScurve>(
        "noisy, two fixed, limited amplitude, start value",
        scurve_data<RisingScurve>(truth, true), true, true, 500, 1e-2,
        [](auto &m) {
            m.FixParameter("sigma", 3.0);
            m.FixParameter("p0", 10.0);
            m.SetParLimits("A", 900.0, 1100.0);
            m.SetParameter("C", 0.0);
        });
}

TEST_CASE("Built-in solver matches the Minuit2 backend on a data cube",
          "[fit][minuit2]") {
    constexpr ssize_t rows = 3;
    constexpr ssize_t cols = 2;
    constexpr ssize_t n = 61;
    NDArray<double, 1> x({n});
    NDArray<double, 3> y({rows, cols, n});
    NDArray<double, 3> y_err({rows, cols, n}, 1.0);
    for (ssize_t row = 0; row < rows; ++row) {
        for (ssize_t col = 0; col < cols; ++col) {
            const double mu = -0.6 + 0.3 * static_cast<double>(col);
            const double sigma = 0.9 + 0.1 * static_cast<double>(row);
            for (ssize_t i = 0; i < n; ++i) {
                x(i) = -6.0 + 0.2 * static_cast<double>(i);
                const double z = (x(i) - mu) / sigma;
                y(row, col, i) = 80.0 * std::exp(-0.5 * z * z) +
                                 2.0 * std::sin(7.3 * static_cast<double>(i));
            }
        }
    }
    NDArray<double, 3> par_lm({rows, cols, 3});
    NDArray<double, 3> err_lm({rows, cols, 3});
    NDArray<double, 2> chi2_lm({rows, cols});
    NDArray<double, 3> par_mn({rows, cols, 3});
    NDArray<double, 3> err_mn({rows, cols, 3});
    NDArray<double, 2> chi2_mn({rows, cols});
    const aare::FitModel<aare::model::Gaussian> model(0, 500, 0.5, true);

    aare::fit_3d(model, x.view(), y.view(), y_err.view(), par_lm.view(),
                 err_lm.view(), chi2_lm.view(), 2);
    aare::minuit2::fit_3d(model, x.view(), y.view(), y_err.view(),
                          par_mn.view(), err_mn.view(), chi2_mn.view(), 2);

    for (ssize_t row = 0; row < rows; ++row) {
        for (ssize_t col = 0; col < cols; ++col) {
            for (ssize_t k = 0; k < 3; ++k) {
                CHECK(par_lm(row, col, k) ==
                      Catch::Approx(par_mn(row, col, k)).epsilon(1e-3));
                CHECK(err_lm(row, col, k) ==
                      Catch::Approx(err_mn(row, col, k)).epsilon(2e-2));
            }
            CHECK(chi2_lm(row, col) ==
                  Catch::Approx(chi2_mn(row, col)).epsilon(1e-4));
        }
    }
}
