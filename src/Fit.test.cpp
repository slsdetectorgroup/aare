// SPDX-License-Identifier: MPL-2.0
#include "aare/Fit.hpp"
#include "aare/FitModel.hpp"
#include "aare/Models.hpp"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <cmath>

namespace {

constexpr ssize_t n_points = 61;

void fill_gaussian(aare::NDArray<double, 1> &x, aare::NDArray<double, 1> &y,
                   double amplitude, double mean, double sigma) {
    for (ssize_t i = 0; i < n_points; ++i) {
        x(i) = -6.0 + 0.2 * static_cast<double>(i);
        const double z = (x(i) - mean) / sigma;
        y(i) = amplitude * std::exp(-0.5 * z * z);
    }
}

} // namespace

TEST_CASE("Minuit2 fits weighted and unweighted Gaussian data", "[fit]") {
    aare::NDArray<double, 1> x({n_points});
    aare::NDArray<double, 1> y({n_points});
    aare::NDArray<double, 1> y_err({n_points}, 1.0);
    fill_gaussian(x, y, 120.0, 0.8, 1.3);

    const aare::FitModel<aare::model::Gaussian> unweighted_model;
    const auto unweighted =
        aare::fit_pixel(unweighted_model, x.view(), y.view());

    REQUIRE(unweighted.size() == 4);
    CHECK(unweighted(0) == Catch::Approx(120.0).epsilon(1e-5));
    CHECK(unweighted(1) == Catch::Approx(0.8).epsilon(1e-5));
    CHECK(unweighted(2) == Catch::Approx(1.3).epsilon(1e-5));
    CHECK(unweighted(3) == Catch::Approx(0.0).margin(1e-4));

    const aare::FitModel<aare::model::Gaussian> weighted_model(0, 100, 0.5,
                                                               true);
    const auto weighted =
        aare::fit_pixel(weighted_model, x.view(), y.view(), y_err.view());

    REQUIRE(weighted.size() == 7);
    CHECK(weighted(0) == Catch::Approx(120.0).epsilon(1e-5));
    CHECK(weighted(1) == Catch::Approx(0.8).epsilon(1e-5));
    CHECK(weighted(2) == Catch::Approx(1.3).epsilon(1e-5));
    CHECK(weighted(6) == Catch::Approx(0.0).margin(1e-4));
}

TEST_CASE("Minuit2 fits a Gaussian data cube in parallel", "[fit]") {
    aare::NDArray<double, 1> x({n_points});
    aare::NDArray<double, 1> values({n_points});
    fill_gaussian(x, values, 80.0, -0.6, 0.9);

    aare::NDArray<double, 3> y({2, 2, n_points});
    for (ssize_t row = 0; row < 2; ++row) {
        for (ssize_t col = 0; col < 2; ++col) {
            for (ssize_t i = 0; i < n_points; ++i)
                y(row, col, i) = values(i);
        }
    }

    aare::NDArray<double, 3> par({2, 2, 3});
    aare::NDArray<double, 2> chi2({2, 2});
    const aare::FitModel<aare::model::Gaussian> model;

    aare::fit_3d(model, x.view(), y.view(), aare::NDView<double, 3>{},
                 par.view(), aare::NDView<double, 3>{}, chi2.view(), 2);

    for (ssize_t row = 0; row < 2; ++row) {
        for (ssize_t col = 0; col < 2; ++col) {
            CHECK(par(row, col, 0) == Catch::Approx(80.0).epsilon(1e-5));
            CHECK(par(row, col, 1) == Catch::Approx(-0.6).epsilon(1e-5));
            CHECK(par(row, col, 2) == Catch::Approx(0.9).epsilon(1e-5));
            CHECK(chi2(row, col) == Catch::Approx(0.0).margin(1e-8));
        }
    }
}
