// SPDX-License-Identifier: MPL-2.0
#include "aare/Fit.hpp"
#include "aare/FitModel.hpp"
#include "aare/Models.hpp"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <cmath>
#include <vector>

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

TEST_CASE("Fumili fits weighted and unweighted Gaussian data", "[fit]") {
    aare::NDArray<double, 1> x({n_points});
    aare::NDArray<double, 1> y({n_points});
    aare::NDArray<double, 1> y_err({n_points}, 1.0);
    fill_gaussian(x, y, 120.0, 0.8, 1.3);

    aare::FitModel<aare::model::Gaussian> unweighted_model;
    REQUIRE(unweighted_model.minimizer() == aare::Minimizer::Migrad);
    unweighted_model.SetMinimizer(aare::Minimizer::Fumili);
    REQUIRE(unweighted_model.minimizer() == aare::Minimizer::Fumili);

    const auto unweighted =
        aare::fit_pixel(unweighted_model, x.view(), y.view());

    REQUIRE(unweighted.size() == 4);
    CHECK(unweighted(0) == Catch::Approx(120.0).epsilon(1e-5));
    CHECK(unweighted(1) == Catch::Approx(0.8).epsilon(1e-5));
    CHECK(unweighted(2) == Catch::Approx(1.3).epsilon(1e-5));
    CHECK(unweighted(3) == Catch::Approx(0.0).margin(1e-4));

    const aare::FitModel<aare::model::Gaussian> weighted_model(
        0, 100, 0.5, true, aare::Minimizer::Fumili);
    const auto weighted =
        aare::fit_pixel(weighted_model, x.view(), y.view(), y_err.view());

    REQUIRE(weighted.size() == 7);
    CHECK(weighted(0) == Catch::Approx(120.0).epsilon(1e-5));
    CHECK(weighted(1) == Catch::Approx(0.8).epsilon(1e-5));
    CHECK(weighted(2) == Catch::Approx(1.3).epsilon(1e-5));
    for (ssize_t k = 3; k < 6; ++k)
        CHECK(weighted(k) > 0.0);
    CHECK(weighted(6) == Catch::Approx(0.0).margin(1e-4));
}

TEST_CASE("Fumili parameter errors agree with Migrad and Hesse", "[fit]") {
    aare::NDArray<double, 1> x({n_points});
    aare::NDArray<double, 1> y({n_points});
    aare::NDArray<double, 1> y_err({n_points}, 2.0);
    fill_gaussian(x, y, 120.0, 0.8, 1.3);

    const aare::FitModel<aare::model::Gaussian> migrad(0, 100, 0.5, true,
                                                       aare::Minimizer::Migrad);
    const aare::FitModel<aare::model::Gaussian> fumili(0, 100, 0.5, true,
                                                       aare::Minimizer::Fumili);

    const auto with_migrad =
        aare::fit_pixel(migrad, x.view(), y.view(), y_err.view());
    const auto with_fumili =
        aare::fit_pixel(fumili, x.view(), y.view(), y_err.view());

    REQUIRE(with_migrad.size() == 7);
    REQUIRE(with_fumili.size() == 7);
    for (ssize_t k = 0; k < 3; ++k)
        CHECK(with_fumili(k) == Catch::Approx(with_migrad(k)).epsilon(1e-5));
    for (ssize_t k = 3; k < 6; ++k)
        CHECK(with_fumili(k) == Catch::Approx(with_migrad(k)).epsilon(1e-2));
}

TEST_CASE("Fumili honours fixed parameters and limits", "[fit]") {
    aare::NDArray<double, 1> x({n_points});
    aare::NDArray<double, 1> y({n_points});
    fill_gaussian(x, y, 120.0, 0.8, 1.3);

    aare::FitModel<aare::model::Gaussian> model(0, 100, 0.5, false,
                                                aare::Minimizer::Fumili);
    model.FixParameter("mu", 0.8);
    model.SetParLimits("sigma", 0.5, 5.0);

    const auto result = aare::fit_pixel(model, x.view(), y.view());

    REQUIRE(result.size() == 4);
    CHECK(result(0) == Catch::Approx(120.0).epsilon(1e-5));
    CHECK(result(1) == 0.8);
    CHECK(result(2) == Catch::Approx(1.3).epsilon(1e-5));
    CHECK(result(3) == Catch::Approx(0.0).margin(1e-4));
}

TEST_CASE("Fumili fits a rising S-curve", "[fit]") {
    // f(x) = (p0 + p1*x) + 0.5*(1 + erf((x - mu)/(sqrt(2)*sigma))) * (A + C*(x
    // - mu))
    const std::vector<double> truth = {5.0, 0.1, 30.0, 4.0, 200.0, 0.5};
    aare::NDArray<double, 1> x({n_points});
    aare::NDArray<double, 1> y({n_points});
    for (ssize_t i = 0; i < n_points; ++i) {
        x(i) = static_cast<double>(i);
        y(i) = aare::model::RisingScurve::eval(x(i), truth);
    }

    const aare::FitModel<aare::model::RisingScurve> model(
        0, 200, 0.5, false, aare::Minimizer::Fumili);
    const auto result = aare::fit_pixel(model, x.view(), y.view());

    REQUIRE(result.size() == 7);
    for (std::size_t k = 0; k < truth.size(); ++k)
        CHECK(result(k) == Catch::Approx(truth[k]).epsilon(1e-3));
}

TEST_CASE("Fumili fits a Gaussian data cube in parallel", "[fit]") {
    aare::NDArray<double, 1> x({n_points});
    aare::NDArray<double, 1> values({n_points});
    fill_gaussian(x, values, 80.0, -0.6, 0.9);

    aare::NDArray<double, 3> y({2, 2, n_points});
    aare::NDArray<double, 3> y_err({2, 2, n_points}, 1.0);
    for (ssize_t row = 0; row < 2; ++row) {
        for (ssize_t col = 0; col < 2; ++col) {
            for (ssize_t i = 0; i < n_points; ++i)
                y(row, col, i) = values(i);
        }
    }

    aare::NDArray<double, 3> par({2, 2, 3});
    aare::NDArray<double, 3> err({2, 2, 3});
    aare::NDArray<double, 2> chi2({2, 2});
    const aare::FitModel<aare::model::Gaussian> model(0, 100, 0.5, true,
                                                      aare::Minimizer::Fumili);

    aare::fit_3d(model, x.view(), y.view(), y_err.view(), par.view(),
                 err.view(), chi2.view(), 2);

    // Fumili stops once the estimated remaining chi2 decrease is below
    // tolerance * 1e-4, which leaves deviations of order 1e-5 relative here.
    for (ssize_t row = 0; row < 2; ++row) {
        for (ssize_t col = 0; col < 2; ++col) {
            CHECK(par(row, col, 0) == Catch::Approx(80.0).epsilon(1e-4));
            CHECK(par(row, col, 1) == Catch::Approx(-0.6).epsilon(1e-4));
            CHECK(par(row, col, 2) == Catch::Approx(0.9).epsilon(1e-4));
            for (ssize_t k = 0; k < 3; ++k)
                CHECK(err(row, col, k) > 0.0);
            CHECK(chi2(row, col) == Catch::Approx(0.0).margin(1e-3));
        }
    }
}

TEST_CASE("Copying a FitModel keeps the minimizer", "[fit]") {
    const aare::FitModel<aare::model::Pol1> original(0, 100, 0.5, false,
                                                     aare::Minimizer::Fumili);
    const aare::FitModel<aare::model::Pol1> copied(original);
    aare::FitModel<aare::model::Pol1> assigned;
    assigned = original;

    CHECK(copied.minimizer() == aare::Minimizer::Fumili);
    CHECK(assigned.minimizer() == aare::Minimizer::Fumili);
}

TEST_CASE("Fitting with every parameter fixed returns the fixed values",
          "[fit]") {
    aare::NDArray<double, 1> x({n_points});
    aare::NDArray<double, 1> y({n_points});
    fill_gaussian(x, y, 120.0, 0.8, 1.3);

    for (const auto minimizer :
         {aare::Minimizer::Migrad, aare::Minimizer::Fumili}) {
        aare::FitModel<aare::model::Gaussian> model(0, 100, 0.5, true,
                                                    minimizer);
        model.FixParameter("A", 120.0);
        model.FixParameter("mu", 0.8);
        model.FixParameter("sigma", 1.3);

        const auto result = aare::fit_pixel(model, x.view(), y.view());

        REQUIRE(result.size() == 7);
        CHECK(result(0) == 120.0);
        CHECK(result(1) == 0.8);
        CHECK(result(2) == 1.3);
        CHECK(result(6) == Catch::Approx(0.0).margin(1e-12));
    }
}
