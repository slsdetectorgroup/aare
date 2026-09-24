// SPDX-License-Identifier: MPL-2.0
#include "aare/Fit.hpp"
#include "aare/FitModel.hpp"
#include "aare/Models.hpp"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <cmath>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>

using aare::NDArray;
using aare::NDView;
using Catch::Approx;

namespace {

constexpr ssize_t n_points = 61;

struct Data {
    NDArray<double, 1> x;
    NDArray<double, 1> y;
    NDArray<double, 1> y_err;
};

void fill_gaussian(NDArray<double, 1> &x, NDArray<double, 1> &y,
                   double amplitude, double mean, double sigma) {
    for (ssize_t i = 0; i < n_points; ++i) {
        x(i) = -6.0 + 0.2 * static_cast<double>(i);
        const double z = (x(i) - mean) / sigma;
        y(i) = amplitude * std::exp(-0.5 * z * z);
    }
}

// Noise-free Gaussian with A = 120, mu = 0.8, sigma = 1.3 and unit errors.
Data gaussian_data() {
    Data d{NDArray<double, 1>({n_points}), NDArray<double, 1>({n_points}),
           NDArray<double, 1>({n_points}, 1.0)};
    fill_gaussian(d.x, d.y, 120.0, 0.8, 1.3);
    return d;
}

// Noise-free S-curve on 100 points in [0, 100] with unit errors.
template <typename Model> Data scurve_data(const std::vector<double> &par) {
    constexpr ssize_t n = 100;
    Data d{NDArray<double, 1>({n}), NDArray<double, 1>({n}),
           NDArray<double, 1>({n}, 1.0)};
    for (ssize_t i = 0; i < n; ++i) {
        d.x(i) = 100.0 * static_cast<double>(i) / static_cast<double>(n - 1);
        d.y(i) = Model::eval(d.x(i), par);
    }
    return d;
}

} // namespace

TEST_CASE("Fit unweighted and weighted noise-free Gaussian data", "[fit]") {
    auto d = gaussian_data();

    const aare::FitModel<aare::model::Gaussian> unweighted_model;
    const auto unweighted =
        aare::fit_pixel(unweighted_model, d.x.view(), d.y.view());

    REQUIRE(unweighted.size() == 4);
    CHECK(unweighted(0) == Approx(120.0).epsilon(1e-4).margin(1e-6));
    CHECK(unweighted(1) == Approx(0.8).epsilon(1e-4).margin(1e-6));
    CHECK(unweighted(2) == Approx(1.3).epsilon(1e-4).margin(1e-6));
    CHECK(unweighted(3) == Approx(0.0).margin(1e-3));

    const aare::FitModel<aare::model::Gaussian> weighted_model(0, 100, 0.5,
                                                               true);
    const auto weighted =
        aare::fit_pixel(weighted_model, d.x.view(), d.y.view(), d.y_err.view());

    REQUIRE(weighted.size() == 7);
    CHECK(weighted(0) == Approx(120.0).epsilon(1e-4).margin(1e-6));
    CHECK(weighted(1) == Approx(0.8).epsilon(1e-4).margin(1e-6));
    CHECK(weighted(2) == Approx(1.3).epsilon(1e-4).margin(1e-6));
    // Gauss-Newton errors equal the exact Hessian errors at zero residual
    CHECK(weighted(3) == Approx(0.361).epsilon(2e-2));
    CHECK(weighted(4) == Approx(0.00451).epsilon(2e-2));
    CHECK(weighted(5) == Approx(0.00451).epsilon(2e-2));
    CHECK(weighted(6) == Approx(0.0).margin(1e-3));
}

TEST_CASE("Fit weighted Pol1 errors match the analytic formula", "[fit]") {
    constexpr ssize_t n = 20;
    NDArray<double, 1> x({n});
    NDArray<double, 1> y({n});
    NDArray<double, 1> y_err({n});
    double S = 0.0, Sx = 0.0, Sxx = 0.0;
    for (ssize_t i = 0; i < n; ++i) {
        x(i) = static_cast<double>(i);
        y(i) = 2.0 + 0.5 * x(i);
        y_err(i) = 1.0 + 0.1 * static_cast<double>(i);
        const double w = 1.0 / (y_err(i) * y_err(i));
        S += w;
        Sx += w * x(i);
        Sxx += w * x(i) * x(i);
    }
    const double delta = S * Sxx - Sx * Sx;

    const aare::FitModel<aare::model::Pol1> model(0, 100, 0.5, true);
    const auto res = aare::fit_pixel(model, x.view(), y.view(), y_err.view());

    REQUIRE(res.size() == 5);
    CHECK(res(0) == Approx(2.0).epsilon(1e-6));
    CHECK(res(1) == Approx(0.5).epsilon(1e-6));
    CHECK(res(2) == Approx(std::sqrt(Sxx / delta)).epsilon(1e-6));
    CHECK(res(3) == Approx(std::sqrt(S / delta)).epsilon(1e-6));
    CHECK(res(4) == Approx(0.0).margin(1e-8));
}

TEST_CASE("Fit with a fixed parameter", "[fit]") {
    auto d = gaussian_data();

    SECTION("fixed at a wrong value") {
        aare::FitModel<aare::model::Gaussian> model(0, 100, 0.5, true);
        model.FixParameter("sigma", 1.5);
        REQUIRE(model.is_user_fixed(2));
        const auto res =
            aare::fit_pixel(model, d.x.view(), d.y.view(), d.y_err.view());
        REQUIRE(res.size() == 7);
        CHECK(res(2) == 1.5);
        CHECK(res(0) == Approx(111.15).epsilon(1e-3));
        CHECK(res(1) == Approx(0.8).epsilon(1e-3));
        CHECK(res(3) == Approx(0.274).epsilon(5e-2));
        CHECK(res(5) == 0.0); // fixed parameters report no error
        CHECK(res(6) == Approx(1684.25).epsilon(1e-3));
    }

    SECTION("fixed at the true value, then released") {
        aare::FitModel<aare::model::Gaussian> model(0, 100, 0.5, true);
        model.FixParameter(2, 1.3);
        auto res =
            aare::fit_pixel(model, d.x.view(), d.y.view(), d.y_err.view());
        CHECK(res(0) == Approx(120.0).epsilon(1e-4).margin(1e-6));
        CHECK(res(1) == Approx(0.8).epsilon(1e-4).margin(1e-6));
        CHECK(res(2) == 1.3);
        CHECK(res(5) == 0.0);
        CHECK(res(6) == Approx(0.0).margin(1e-3));

        model.ReleaseParameter("sigma");
        REQUIRE_FALSE(model.is_user_fixed(2));
        res = aare::fit_pixel(model, d.x.view(), d.y.view(), d.y_err.view());
        CHECK(res(2) == Approx(1.3).epsilon(1e-4).margin(1e-6));
        CHECK(res(5) == Approx(0.00451).epsilon(2e-2));
    }
}

TEST_CASE("Fit with parameter limits", "[fit]") {
    auto d = gaussian_data();

    SECTION("active upper limit on mu") {
        aare::FitModel<aare::model::Gaussian> model;
        model.SetParLimits("mu", -5.0, 0.5);
        const auto res = aare::fit_pixel(model, d.x.view(), d.y.view());
        CHECK(res(1) == Approx(0.5).margin(1e-9));
        CHECK(res(0) == Approx(116.88).epsilon(1e-3));
        CHECK(res(2) == Approx(1.335).epsilon(1e-3));
        CHECK(res(3) == Approx(4301.95).epsilon(1e-3));
    }

    SECTION("active upper limit on sigma, with errors") {
        aare::FitModel<aare::model::Gaussian> model(0, 100, 0.5, true);
        model.SetParLimits(2, 0.5, 1.0);
        const auto res =
            aare::fit_pixel(model, d.x.view(), d.y.view(), d.y_err.view());
        CHECK(res(2) == Approx(1.0).margin(1e-9));
        CHECK(res(1) == Approx(0.8).epsilon(1e-3));
        CHECK(res(3) == Approx(0.336).epsilon(5e-2));
        CHECK(res(5) == 0.0); // on a limit: no error reported
        CHECK(res(6) == Approx(5550.61).epsilon(1e-3));
    }

    SECTION("fixed amplitude and an active limit on mu") {
        aare::FitModel<aare::model::Gaussian> model(0, 100, 0.5, true);
        model.FixParameter("A", 100.0);
        model.SetParLimits("mu", 0.6, 0.7);
        const auto res =
            aare::fit_pixel(model, d.x.view(), d.y.view(), d.y_err.view());
        CHECK(res(0) == 100.0);
        CHECK(res(1) == Approx(0.7).margin(1e-9));
        CHECK(res(2) == Approx(1.4603).epsilon(1e-3));
        CHECK(res(3) == 0.0);
        CHECK(res(4) == 0.0);
        CHECK(res(5) == Approx(0.00469).epsilon(5e-2));
        CHECK(res(6) == Approx(3631.01).epsilon(1e-3));
    }

    SECTION("inactive limits do not change the result") {
        const aare::FitModel<aare::model::Gaussian> free_model;
        const auto reference =
            aare::fit_pixel(free_model, d.x.view(), d.y.view());

        aare::FitModel<aare::model::Gaussian> model;
        model.SetParLimits("mu", -5.0, 5.0);
        model.SetParLimits("sigma", 0.5,
                           std::numeric_limits<double>::infinity());
        const auto res = aare::fit_pixel(model, d.x.view(), d.y.view());
        for (ssize_t k = 0; k < 3; ++k)
            CHECK(res(k) == Approx(reference(k)).epsilon(1e-4).margin(1e-6));
    }
}

TEST_CASE("Fit honours user start values", "[fit]") {
    auto d = gaussian_data();

    SECTION("start value for mu") {
        aare::FitModel<aare::model::Gaussian> model;
        model.SetParameter("mu", 0.0);
        REQUIRE(model.is_user_start(1));
        CHECK(model.value(1) == 0.0);
        const auto res = aare::fit_pixel(model, d.x.view(), d.y.view());
        CHECK(res(0) == Approx(120.0).epsilon(1e-4).margin(1e-6));
        CHECK(res(1) == Approx(0.8).epsilon(1e-4).margin(1e-6));
        CHECK(res(2) == Approx(1.3).epsilon(1e-4).margin(1e-6));
    }

    SECTION("negative Gaussian with all start values given") {
        Data n{NDArray<double, 1>({n_points}), NDArray<double, 1>({n_points}),
               NDArray<double, 1>({n_points}, 1.0)};
        fill_gaussian(n.x, n.y, -120.0, 0.8, 1.3);
        aare::FitModel<aare::model::Gaussian> model;
        model.SetParameter("A", -100.0);
        model.SetParameter("mu", 0.5);
        model.SetParameter("sigma", 1.0);
        const auto res = aare::fit_pixel(model, n.x.view(), n.y.view());
        CHECK(res(0) == Approx(-120.0).epsilon(1e-4).margin(1e-6));
        CHECK(res(1) == Approx(0.8).epsilon(1e-4).margin(1e-6));
        CHECK(res(2) == Approx(1.3).epsilon(1e-4).margin(1e-6));
    }
}

TEST_CASE("Fit reports failure as zeros when max_calls is exhausted", "[fit]") {
    auto d = gaussian_data();

    const aare::FitModel<aare::model::Gaussian> model(0, 3, 0.5, false);
    const auto res = aare::fit_pixel(model, d.x.view(), d.y.view());
    REQUIRE(res.size() == 4);
    for (ssize_t k = 0; k < res.size(); ++k)
        CHECK(res(k) == 0.0);

    const aare::FitModel<aare::model::Gaussian> model_err(0, 3, 0.5, true);
    const auto res_err =
        aare::fit_pixel(model_err, d.x.view(), d.y.view(), d.y_err.view());
    REQUIRE(res_err.size() == 7);
    for (ssize_t k = 0; k < res_err.size(); ++k)
        CHECK(res_err(k) == 0.0);
}

TEST_CASE("FitModel validates parameter names, indices and limits", "[fit]") {
    aare::FitModel<aare::model::Gaussian> model;
    CHECK(model.GetParNames() == std::vector<std::string>{"A", "mu", "sigma"});
    CHECK(model.GetParName(2) == "sigma");
    CHECK(model.GetNpar() == 3);
    CHECK(model.strategy() == 0);
    CHECK(model.lower_limit(2) == 1e-12);
    CHECK(std::isinf(model.upper_limit(2)));

    CHECK_THROWS_AS(model.SetParameter("nope", 1.0), std::runtime_error);
    CHECK_THROWS_AS(model.SetParameter(3, 1.0), std::out_of_range);
    CHECK_THROWS_AS(model.GetParName(5), std::out_of_range);
    CHECK_THROWS_AS(model.SetParLimits(0, 1.0, 1.0), std::runtime_error);
    CHECK_THROWS_AS(model.FixParameter("nope", 1.0), std::runtime_error);
    CHECK_NOTHROW(model.SetParLimits("mu", -1.0, 1.0));
    CHECK(model.lower_limit(1) == -1.0);
    CHECK(model.upper_limit(1) == 1.0);
}

TEST_CASE("Fit rising and falling S-curves", "[fit]") {
    const std::vector<double> truth{10.0, 0.1, 50.0, 3.0, 1000.0, 2.0};

    SECTION("rising, free fit with errors") {
        auto d = scurve_data<aare::model::RisingScurve>(truth);
        const aare::FitModel<aare::model::RisingScurve> model(0, 500, 0.5,
                                                              true);
        const auto res =
            aare::fit_pixel(model, d.x.view(), d.y.view(), d.y_err.view());
        REQUIRE(res.size() == 13);
        for (std::size_t k = 0; k < truth.size(); ++k) {
            CHECK(res(static_cast<ssize_t>(k)) ==
                  Approx(truth[k]).epsilon(1e-3));
            CHECK(res(static_cast<ssize_t>(6 + k)) > 0.0);
        }
        CHECK(res(12) == Approx(0.0).margin(1e-3));
    }

    SECTION("rising, fixed slope and limited width") {
        auto d = scurve_data<aare::model::RisingScurve>(truth);
        aare::FitModel<aare::model::RisingScurve> model(0, 500, 0.5, true);
        model.FixParameter("p1", 0.1);
        model.SetParLimits("sigma", 1.0, 4.0);
        const auto res =
            aare::fit_pixel(model, d.x.view(), d.y.view(), d.y_err.view());
        CHECK(res(1) == 0.1);
        CHECK(res(7) == 0.0);
        CHECK(res(2) == Approx(50.0).epsilon(1e-3));
        CHECK(res(3) == Approx(3.0).epsilon(1e-3));
        CHECK(res(9) > 0.0);
    }

    SECTION("rising, active limit on the threshold") {
        auto d = scurve_data<aare::model::RisingScurve>(truth);
        aare::FitModel<aare::model::RisingScurve> model(0, 500, 0.5, false);
        model.SetParLimits("mu", 40.0, 48.0);
        const auto res = aare::fit_pixel(model, d.x.view(), d.y.view());
        CHECK(res(2) == Approx(48.0).margin(1e-9));
        CHECK(res(6) > 0.0); // chi2 of the constrained fit
    }

    SECTION("falling") {
        auto d = scurve_data<aare::model::FallingScurve>(truth);
        const aare::FitModel<aare::model::FallingScurve> model(0, 500, 0.5,
                                                               false);
        const auto res = aare::fit_pixel(model, d.x.view(), d.y.view());
        REQUIRE(res.size() == 7);
        for (std::size_t k = 0; k < truth.size(); ++k)
            CHECK(res(static_cast<ssize_t>(k)) ==
                  Approx(truth[k]).epsilon(1e-3));
    }
}

TEST_CASE("Fit a Gaussian data cube in parallel", "[fit]") {
    NDArray<double, 1> x({n_points});
    NDArray<double, 1> values({n_points});
    fill_gaussian(x, values, 80.0, -0.6, 0.9);

    NDArray<double, 3> y({2, 2, n_points});
    for (ssize_t row = 0; row < 2; ++row) {
        for (ssize_t col = 0; col < 2; ++col) {
            for (ssize_t i = 0; i < n_points; ++i)
                y(row, col, i) = values(i);
        }
    }

    NDArray<double, 3> par({2, 2, 3});
    NDArray<double, 2> chi2({2, 2});
    const aare::FitModel<aare::model::Gaussian> model;

    aare::fit_3d(model, x.view(), y.view(), NDView<double, 3>{}, par.view(),
                 NDView<double, 3>{}, chi2.view(), 2);

    for (ssize_t row = 0; row < 2; ++row) {
        for (ssize_t col = 0; col < 2; ++col) {
            CHECK(par(row, col, 0) == Approx(80.0).epsilon(1e-4).margin(1e-6));
            CHECK(par(row, col, 1) == Approx(-0.6).epsilon(1e-4).margin(1e-6));
            CHECK(par(row, col, 2) == Approx(0.9).epsilon(1e-4).margin(1e-6));
            CHECK(chi2(row, col) == Approx(0.0).margin(1e-3));
        }
    }

    CHECK_THROWS_AS(aare::fit_3d(model, NDView<double, 1>(x.data(), {10}),
                                 y.view(), NDView<double, 3>{}, par.view(),
                                 NDView<double, 3>{}, chi2.view(), 2),
                    std::runtime_error);
}

TEST_CASE("Fit a data cube with errors matches per-pixel fits", "[fit]") {
    constexpr ssize_t rows = 4;
    constexpr ssize_t cols = 3;
    NDArray<double, 1> x({n_points});
    NDArray<double, 3> y({rows, cols, n_points});
    NDArray<double, 3> y_err({rows, cols, n_points}, 1.0);
    NDArray<double, 1> tmp({n_points});
    for (ssize_t row = 0; row < rows; ++row) {
        for (ssize_t col = 0; col < cols; ++col) {
            fill_gaussian(x, tmp, 80.0 + 10.0 * static_cast<double>(row),
                          -0.6 + 0.3 * static_cast<double>(col),
                          0.9 + 0.1 * static_cast<double>(row));
            for (ssize_t i = 0; i < n_points; ++i)
                y(row, col, i) = tmp(i);
        }
    }

    NDArray<double, 3> par({rows, cols, 3});
    NDArray<double, 3> err({rows, cols, 3});
    NDArray<double, 2> chi2({rows, cols});
    const aare::FitModel<aare::model::Gaussian> model(0, 100, 0.5, true);

    aare::fit_3d(model, x.view(), y.view(), y_err.view(), par.view(),
                 err.view(), chi2.view(), 3);

    for (ssize_t row = 0; row < rows; ++row) {
        for (ssize_t col = 0; col < cols; ++col) {
            NDView<double, 1> yv(&y(row, col, 0), {n_points});
            NDView<double, 1> ev(&y_err(row, col, 0), {n_points});
            const auto single = aare::fit_pixel(model, x.view(), yv, ev);
            // reused solver buffers must not leak state between pixels
            for (ssize_t k = 0; k < 3; ++k) {
                CHECK(par(row, col, k) == Approx(single(k)).margin(1e-12));
                CHECK(err(row, col, k) == Approx(single(3 + k)).margin(1e-12));
                CHECK(err(row, col, k) > 0.0);
            }
            CHECK(chi2(row, col) == Approx(single(6)).margin(1e-12));
            CHECK(par(row, col, 0) ==
                  Approx(80.0 + 10.0 * static_cast<double>(row))
                      .epsilon(1e-4)
                      .margin(1e-6));
            CHECK(par(row, col, 1) ==
                  Approx(-0.6 + 0.3 * static_cast<double>(col))
                      .epsilon(1e-4)
                      .margin(1e-6));
            CHECK(par(row, col, 2) ==
                  Approx(0.9 + 0.1 * static_cast<double>(row))
                      .epsilon(1e-4)
                      .margin(1e-6));
        }
    }
}
