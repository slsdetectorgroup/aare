// SPDX-License-Identifier: MPL-2.0
#include "aare/Fit.hpp"
#include "aare/FitModel.hpp"
#include "aare/Models.hpp"

#include <algorithm>
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <cmath>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>

using aare::Minimizer;
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

// Gaussian with A = 120, mu = 0.8, sigma = 1.3 and unit errors, optionally
// with deterministic pseudo-noise.
Data gaussian_data(bool add_noise = false) {
    Data d{NDArray<double, 1>({n_points}), NDArray<double, 1>({n_points}),
           NDArray<double, 1>({n_points}, 1.0)};
    fill_gaussian(d.x, d.y, 120.0, 0.8, 1.3);
    if (add_noise) {
        for (ssize_t i = 0; i < n_points; ++i)
            d.y(i) += 3.0 * std::sin(7.3 * static_cast<double>(i));
    }
    return d;
}

// S-curve on 100 points in [0, 100] with unit errors, optionally with
// deterministic pseudo-noise and errors of 30.
template <typename Model>
Data scurve_data(const std::vector<double> &par, bool add_noise = false) {
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

const char *name(Minimizer m) {
    switch (m) {
    case Minimizer::Migrad:
        return "Migrad";
    case Minimizer::Fumili:
        return "Fumili";
    case Minimizer::LevenbergMarquardt:
        return "LevenbergMarquardt";
    }
    return "unknown";
}

bool uses_minuit2(Minimizer m) { return m != Minimizer::LevenbergMarquardt; }

template <typename Model>
aare::FitModel<Model> make_model(Minimizer minimizer, bool errors = false,
                                 unsigned int max_calls = 100) {
    return aare::FitModel<Model>(0, max_calls, 0.5, errors, minimizer);
}

// Minuit2 reaches an active limit through its parameter transformation;
// the built-in solver lands on it exactly.
double limit_margin(Minimizer m) { return uses_minuit2(m) ? 1e-6 : 1e-9; }

// Relative agreement with the true parameters: the Minuit2 minimizers stop
// at Minuit's EDM tolerance, the built-in solver lands tighter.
double truth_tolerance(Minimizer m) { return uses_minuit2(m) ? 1e-3 : 1e-4; }

// Hesse errors at a point forced away from the free minimum differ from the
// Gauss-Newton estimates of Fumili and the built-in solver.
double misfit_error_tolerance(Minimizer m) {
    return uses_minuit2(m) ? 0.1 : 5e-2;
}

template <typename Model>
bool on_limit(const aare::FitModel<Model> &model, unsigned int idx,
              double value) {
    const double margin = 1e-6 * std::max(1.0, std::abs(value));
    return std::abs(value - model.lower_limit(idx)) <= margin ||
           std::abs(value - model.upper_limit(idx)) <= margin;
}

/**
 * Run one configuration through every minimizer and compare Fumili and the
 * built-in solver against Migrad: parameters and chi2 within tol (relative),
 * unless the candidate found a clearly lower chi2. Errors are compared for
 * free parameters that do not end on a limit with tolerance err_tol.
 */
template <typename Model, typename Setup>
void check_minimizers_agree(const std::string &label, Data d, bool weighted,
                            bool errors, unsigned int max_calls, double tol,
                            Setup setup, double err_tol = 0.0) {
    if (err_tol == 0.0)
        err_tol = 10.0 * tol;
    constexpr std::size_t npar = Model::npar;

    const auto run = [&](Minimizer minimizer) {
        aare::FitModel<Model> model(0, max_calls, 0.5, errors, minimizer);
        setup(model);
        return weighted ? aare::fit_pixel(model, d.x.view(), d.y.view(),
                                          d.y_err.view())
                        : aare::fit_pixel(model, d.x.view(), d.y.view());
    };
    aare::FitModel<Model> reference_model(0, max_calls, 0.5, errors,
                                          Minimizer::Migrad);
    setup(reference_model);
    const auto ref = run(Minimizer::Migrad);

    for (const auto candidate :
         {Minimizer::Fumili, Minimizer::LevenbergMarquardt}) {
        INFO(label << ": " << name(candidate) << " vs Migrad");
        const auto res = run(candidate);
        REQUIRE(res.size() == ref.size());

        const auto ichi = static_cast<ssize_t>(errors ? 2 * npar : npar);
        const double chi_ref = ref(ichi);
        const double chi_res = res(ichi);
        if (chi_ref > 0.0 && chi_res > 0.0 &&
            chi_ref - chi_res > tol * std::max(chi_ref, 1.0)) {
            WARN(label + ": " + name(candidate) + " found a lower chi2 (" +
                 std::to_string(chi_res) + " vs " + std::to_string(chi_ref) +
                 ")");
            continue;
        }
        double scale = 0.0;
        for (std::size_t k = 0; k < npar; ++k)
            scale = std::max(scale, std::abs(ref(static_cast<ssize_t>(k))));
        for (std::size_t k = 0; k < npar; ++k) {
            const auto i = static_cast<ssize_t>(k);
            const double denom =
                std::max({std::abs(ref(i)), 1e-2 * scale, 1e-12});
            INFO("parameter "
                 << reference_model.GetParName(static_cast<unsigned int>(k)));
            CHECK(std::abs(res(i) - ref(i)) / denom <= tol);
        }
        CHECK(std::abs(chi_res - chi_ref) / std::max(std::abs(chi_ref), 1.0) <=
              tol);
        if (!errors)
            continue;
        for (std::size_t k = 0; k < npar; ++k) {
            const auto idx = static_cast<unsigned int>(k);
            const auto i = static_cast<ssize_t>(k);
            if (reference_model.is_user_fixed(idx) ||
                on_limit(reference_model, idx, ref(i)))
                continue;
            const auto ie = static_cast<ssize_t>(npar + k);
            INFO("error of " << reference_model.GetParName(idx));
            CHECK(std::abs(res(ie) - ref(ie)) /
                      std::max(std::abs(ref(ie)), 1e-12) <=
                  err_tol);
        }
    }
}

} // namespace

TEST_CASE("Fit unweighted and weighted noise-free Gaussian data", "[fit]") {
    const auto minimizer = GENERATE(Minimizer::Migrad, Minimizer::Fumili,
                                    Minimizer::LevenbergMarquardt);
    INFO("minimizer " << name(minimizer));
    auto d = gaussian_data();

    const auto unweighted_model = make_model<aare::model::Gaussian>(minimizer);
    const auto unweighted =
        aare::fit_pixel(unweighted_model, d.x.view(), d.y.view());

    REQUIRE(unweighted.size() == 4);
    CHECK(unweighted(0) == Approx(120.0).epsilon(1e-4).margin(1e-6));
    CHECK(unweighted(1) == Approx(0.8).epsilon(1e-4).margin(1e-6));
    CHECK(unweighted(2) == Approx(1.3).epsilon(1e-4).margin(1e-6));
    CHECK(unweighted(3) == Approx(0.0).margin(1e-3));

    const auto weighted_model =
        make_model<aare::model::Gaussian>(minimizer, true);
    const auto weighted =
        aare::fit_pixel(weighted_model, d.x.view(), d.y.view(), d.y_err.view());

    REQUIRE(weighted.size() == 7);
    CHECK(weighted(0) == Approx(120.0).epsilon(1e-4).margin(1e-6));
    CHECK(weighted(1) == Approx(0.8).epsilon(1e-4).margin(1e-6));
    CHECK(weighted(2) == Approx(1.3).epsilon(1e-4).margin(1e-6));
    // At zero residual the Hesse, Fumili and Gauss-Newton errors coincide.
    CHECK(weighted(3) == Approx(0.361).epsilon(2e-2));
    CHECK(weighted(4) == Approx(0.00451).epsilon(2e-2));
    CHECK(weighted(5) == Approx(0.00451).epsilon(2e-2));
    CHECK(weighted(6) == Approx(0.0).margin(1e-3));
}

TEST_CASE("Fit weighted Pol1 errors match the analytic formula", "[fit]") {
    const auto minimizer = GENERATE(Minimizer::Migrad, Minimizer::Fumili,
                                    Minimizer::LevenbergMarquardt);
    INFO("minimizer " << name(minimizer));
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

    const auto model = make_model<aare::model::Pol1>(minimizer, true);
    const auto res = aare::fit_pixel(model, x.view(), y.view(), y_err.view());

    REQUIRE(res.size() == 5);
    CHECK(res(0) == Approx(2.0).epsilon(1e-6));
    CHECK(res(1) == Approx(0.5).epsilon(1e-6));
    CHECK(res(2) == Approx(std::sqrt(Sxx / delta)).epsilon(1e-6));
    CHECK(res(3) == Approx(std::sqrt(S / delta)).epsilon(1e-6));
    CHECK(res(4) == Approx(0.0).margin(1e-8));
}

TEST_CASE("Fit with a fixed parameter", "[fit]") {
    const auto minimizer = GENERATE(Minimizer::Migrad, Minimizer::Fumili,
                                    Minimizer::LevenbergMarquardt);
    INFO("minimizer " << name(minimizer));
    auto d = gaussian_data();

    SECTION("fixed at a wrong value") {
        auto model = make_model<aare::model::Gaussian>(minimizer, true);
        model.FixParameter("sigma", 1.5);
        REQUIRE(model.is_user_fixed(2));
        const auto res =
            aare::fit_pixel(model, d.x.view(), d.y.view(), d.y_err.view());
        REQUIRE(res.size() == 7);
        CHECK(res(2) == 1.5);
        CHECK(res(0) == Approx(111.15).epsilon(1e-3));
        CHECK(res(1) == Approx(0.8).epsilon(1e-3));
        CHECK(res(3) ==
              Approx(0.274).epsilon(misfit_error_tolerance(minimizer)));
        CHECK(res(5) == 0.0); // fixed parameters report no error
        CHECK(res(6) == Approx(1684.25).epsilon(1e-3));
    }

    SECTION("fixed at the true value, then released") {
        auto model = make_model<aare::model::Gaussian>(minimizer, true);
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
    const auto minimizer = GENERATE(Minimizer::Migrad, Minimizer::Fumili,
                                    Minimizer::LevenbergMarquardt);
    INFO("minimizer " << name(minimizer));
    auto d = gaussian_data();
    const double margin = limit_margin(minimizer);
    const double err_tol = misfit_error_tolerance(minimizer);

    SECTION("active upper limit on mu") {
        auto model = make_model<aare::model::Gaussian>(minimizer);
        model.SetParLimits("mu", -5.0, 0.5);
        const auto res = aare::fit_pixel(model, d.x.view(), d.y.view());
        CHECK(res(1) == Approx(0.5).margin(margin));
        CHECK(res(0) == Approx(116.88).epsilon(1e-3));
        CHECK(res(2) == Approx(1.335).epsilon(1e-3));
        CHECK(res(3) == Approx(4301.95).epsilon(1e-3));
    }

    SECTION("active upper limit on sigma, with errors") {
        auto model = make_model<aare::model::Gaussian>(minimizer, true);
        model.SetParLimits(2, 0.5, 1.0);
        const auto res =
            aare::fit_pixel(model, d.x.view(), d.y.view(), d.y_err.view());
        CHECK(res(2) == Approx(1.0).margin(margin));
        CHECK(res(1) == Approx(0.8).epsilon(1e-3));
        CHECK(res(3) == Approx(0.336).epsilon(err_tol));
        CHECK(res(5) == 0.0); // on a limit: no error reported
        CHECK(res(6) == Approx(5550.61).epsilon(1e-3));
    }

    SECTION("fixed amplitude and an active limit on mu") {
        auto model = make_model<aare::model::Gaussian>(minimizer, true);
        model.FixParameter("A", 100.0);
        model.SetParLimits("mu", 0.6, 0.7);
        const auto res =
            aare::fit_pixel(model, d.x.view(), d.y.view(), d.y_err.view());
        CHECK(res(0) == 100.0);
        CHECK(res(1) == Approx(0.7).margin(margin));
        CHECK(res(2) == Approx(1.4603).epsilon(1e-3));
        CHECK(res(3) == 0.0);
        CHECK(res(4) == 0.0);
        CHECK(res(5) == Approx(0.00469).epsilon(err_tol));
        CHECK(res(6) == Approx(3631.01).epsilon(1e-3));
    }

    SECTION("inactive limits do not change the result") {
        const auto free_model = make_model<aare::model::Gaussian>(minimizer);
        const auto reference =
            aare::fit_pixel(free_model, d.x.view(), d.y.view());

        auto model = make_model<aare::model::Gaussian>(minimizer);
        model.SetParLimits("mu", -5.0, 5.0);
        model.SetParLimits("sigma", 0.5,
                           std::numeric_limits<double>::infinity());
        const auto res = aare::fit_pixel(model, d.x.view(), d.y.view());
        for (ssize_t k = 0; k < 3; ++k)
            CHECK(res(k) == Approx(reference(k)).epsilon(1e-4).margin(1e-6));
    }
}

TEST_CASE("Fit honours user start values", "[fit]") {
    const auto minimizer = GENERATE(Minimizer::Migrad, Minimizer::Fumili,
                                    Minimizer::LevenbergMarquardt);
    INFO("minimizer " << name(minimizer));
    auto d = gaussian_data();

    SECTION("start value for mu") {
        auto model = make_model<aare::model::Gaussian>(minimizer);
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
        auto model = make_model<aare::model::Gaussian>(minimizer);
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
    const auto minimizer = GENERATE(Minimizer::Migrad, Minimizer::Fumili,
                                    Minimizer::LevenbergMarquardt);
    INFO("minimizer " << name(minimizer));
    auto d = gaussian_data();

    // Start far from the minimum so that no minimizer converges in 3 calls.
    auto model = make_model<aare::model::Gaussian>(minimizer, false, 3);
    model.SetParameter("mu", -3.0);
    model.SetParameter("sigma", 3.0);
    const auto res = aare::fit_pixel(model, d.x.view(), d.y.view());
    REQUIRE(res.size() == 4);
    for (ssize_t k = 0; k < res.size(); ++k)
        CHECK(res(k) == 0.0);

    auto model_err = make_model<aare::model::Gaussian>(minimizer, true, 3);
    model_err.SetParameter("mu", -3.0);
    model_err.SetParameter("sigma", 3.0);
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
    CHECK(model.minimizer() == Minimizer::Migrad);
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

    model.SetMinimizer(Minimizer::LevenbergMarquardt);
    CHECK(model.minimizer() == Minimizer::LevenbergMarquardt);
}

TEST_CASE("Copying a FitModel keeps the minimizer", "[fit]") {
    const aare::FitModel<aare::model::Pol1> original(
        0, 100, 0.5, false, Minimizer::LevenbergMarquardt);
    const aare::FitModel<aare::model::Pol1> copied(original);
    aare::FitModel<aare::model::Pol1> assigned;
    assigned = original;

    CHECK(copied.minimizer() == Minimizer::LevenbergMarquardt);
    CHECK(assigned.minimizer() == Minimizer::LevenbergMarquardt);
}

TEST_CASE("Fit rising and falling S-curves", "[fit]") {
    const auto minimizer = GENERATE(Minimizer::Migrad, Minimizer::Fumili,
                                    Minimizer::LevenbergMarquardt);
    INFO("minimizer " << name(minimizer));
    const std::vector<double> truth{10.0, 0.1, 50.0, 3.0, 1000.0, 2.0};

    SECTION("rising, free fit with errors") {
        auto d = scurve_data<aare::model::RisingScurve>(truth);
        const auto model =
            make_model<aare::model::RisingScurve>(minimizer, true, 500);
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
        auto model =
            make_model<aare::model::RisingScurve>(minimizer, true, 500);
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
        auto model =
            make_model<aare::model::RisingScurve>(minimizer, false, 500);
        model.SetParLimits("mu", 40.0, 48.0);
        const auto res = aare::fit_pixel(model, d.x.view(), d.y.view());
        CHECK(res(2) == Approx(48.0).margin(limit_margin(minimizer)));
        CHECK(res(6) > 0.0); // chi2 of the constrained fit
    }

    SECTION("falling") {
        auto d = scurve_data<aare::model::FallingScurve>(truth);
        const auto model =
            make_model<aare::model::FallingScurve>(minimizer, false, 500);
        const auto res = aare::fit_pixel(model, d.x.view(), d.y.view());
        REQUIRE(res.size() == 7);
        for (std::size_t k = 0; k < truth.size(); ++k)
            CHECK(res(static_cast<ssize_t>(k)) ==
                  Approx(truth[k]).epsilon(1e-3));
    }
}

TEST_CASE("Fit a Gaussian data cube in parallel", "[fit]") {
    const auto minimizer = GENERATE(Minimizer::Migrad, Minimizer::Fumili,
                                    Minimizer::LevenbergMarquardt);
    INFO("minimizer " << name(minimizer));
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
    const auto model = make_model<aare::model::Gaussian>(minimizer);

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
    const auto minimizer = GENERATE(Minimizer::Migrad, Minimizer::Fumili,
                                    Minimizer::LevenbergMarquardt);
    INFO("minimizer " << name(minimizer));
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
    const auto model = make_model<aare::model::Gaussian>(minimizer, true);

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
            const double tol = truth_tolerance(minimizer);
            CHECK(par(row, col, 0) ==
                  Approx(80.0 + 10.0 * static_cast<double>(row))
                      .epsilon(tol)
                      .margin(1e-6));
            CHECK(par(row, col, 1) ==
                  Approx(-0.6 + 0.3 * static_cast<double>(col))
                      .epsilon(tol)
                      .margin(1e-6));
            CHECK(par(row, col, 2) ==
                  Approx(0.9 + 0.1 * static_cast<double>(row))
                      .epsilon(tol)
                      .margin(1e-6));
        }
    }
}

TEST_CASE("Fitting with every parameter fixed returns the fixed values",
          "[fit]") {
    const auto minimizer = GENERATE(Minimizer::Migrad, Minimizer::Fumili,
                                    Minimizer::LevenbergMarquardt);
    INFO("minimizer " << name(minimizer));
    auto d = gaussian_data();

    auto model = make_model<aare::model::Gaussian>(minimizer, true);
    model.FixParameter("A", 120.0);
    model.FixParameter("mu", 0.8);
    model.FixParameter("sigma", 1.3);

    const auto result = aare::fit_pixel(model, d.x.view(), d.y.view());

    REQUIRE(result.size() == 7);
    CHECK(result(0) == 120.0);
    CHECK(result(1) == 0.8);
    CHECK(result(2) == 1.3);
    for (ssize_t k = 3; k < 6; ++k)
        CHECK(result(k) == 0.0);
    CHECK(result(6) == Approx(0.0).margin(1e-12));
}

TEST_CASE("All minimizers agree on Gaussian fits", "[fit]") {
    using aare::model::Gaussian;
    const auto none = [](auto &) {};
    check_minimizers_agree<Gaussian>("exact, unweighted", gaussian_data(),
                                     false, false, 100, 1e-4, none);
    check_minimizers_agree<Gaussian>("exact, weighted with errors",
                                     gaussian_data(), true, true, 100, 1e-4,
                                     none);
    check_minimizers_agree<Gaussian>(
        "sigma fixed at a wrong value", gaussian_data(), true, true, 100, 1e-3,
        [](auto &m) { m.FixParameter("sigma", 1.5); }, 0.2);
    check_minimizers_agree<Gaussian>(
        "sigma fixed at the true value", gaussian_data(), true, true, 100, 1e-3,
        [](auto &m) { m.FixParameter("sigma", 1.3); });
    check_minimizers_agree<Gaussian>(
        "active upper limit on mu", gaussian_data(), false, false, 100, 1e-3,
        [](auto &m) { m.SetParLimits("mu", -5.0, 0.5); });
    check_minimizers_agree<Gaussian>(
        "inactive limits", gaussian_data(), false, false, 100, 1e-4,
        [](auto &m) { m.SetParLimits("mu", -5.0, 5.0); });
    check_minimizers_agree<Gaussian>(
        "active upper limit on sigma", gaussian_data(), true, true, 100, 1e-3,
        [](auto &m) { m.SetParLimits("sigma", 0.5, 1.0); }, 0.2);
    check_minimizers_agree<Gaussian>(
        "user start value", gaussian_data(), false, false, 100, 1e-4,
        [](auto &m) { m.SetParameter("mu", 0.0); });
    check_minimizers_agree<Gaussian>(
        "fixed amplitude and active limit on mu", gaussian_data(), true, true,
        100, 1e-3,
        [](auto &m) {
            m.FixParameter("A", 100.0);
            m.SetParLimits("mu", 0.6, 0.7);
        },
        0.2);
    check_minimizers_agree<Gaussian>("noisy, weighted with errors",
                                     gaussian_data(true), true, true, 500, 1e-3,
                                     none);
    check_minimizers_agree<Gaussian>(
        "noisy, fixed mu and active limit on sigma", gaussian_data(true), true,
        true, 500, 1e-3,
        [](auto &m) {
            m.FixParameter("mu", 0.8);
            m.SetParLimits("sigma", 0.5, 1.1);
        },
        0.2);
}

TEST_CASE("All minimizers agree on S-curve fits", "[fit]") {
    using aare::model::RisingScurve;
    const std::vector<double> truth{10.0, 0.1, 50.0, 3.0, 1000.0, 2.0};
    const auto none = [](auto &) {};
    check_minimizers_agree<RisingScurve>("noisy, weighted with errors",
                                         scurve_data<RisingScurve>(truth, true),
                                         true, true, 500, 2e-2, none);
    check_minimizers_agree<RisingScurve>("noisy, fixed slope and limited width",
                                         scurve_data<RisingScurve>(truth, true),
                                         true, true, 500, 1e-2, [](auto &m) {
                                             m.FixParameter("p1", 0.1);
                                             m.SetParLimits("sigma", 1.0, 4.0);
                                         });
    check_minimizers_agree<RisingScurve>(
        "noisy, active limit on the threshold",
        scurve_data<RisingScurve>(truth, true), false, false, 500, 1e-2,
        [](auto &m) { m.SetParLimits("mu", 40.0, 48.0); });
    check_minimizers_agree<RisingScurve>(
        "noisy, two fixed, limited amplitude, start value",
        scurve_data<RisingScurve>(truth, true), true, true, 500, 1e-2,
        [](auto &m) {
            m.FixParameter("sigma", 3.0);
            m.FixParameter("p0", 10.0);
            m.SetParLimits("A", 900.0, 1100.0);
            m.SetParameter("C", 0.0);
        });
}

TEST_CASE("All minimizers agree on a noisy data cube", "[fit]") {
    constexpr ssize_t rows = 3;
    constexpr ssize_t cols = 2;
    NDArray<double, 1> x({n_points});
    NDArray<double, 3> y({rows, cols, n_points});
    NDArray<double, 3> y_err({rows, cols, n_points}, 1.0);
    for (ssize_t row = 0; row < rows; ++row) {
        for (ssize_t col = 0; col < cols; ++col) {
            const double mu = -0.6 + 0.3 * static_cast<double>(col);
            const double sigma = 0.9 + 0.1 * static_cast<double>(row);
            for (ssize_t i = 0; i < n_points; ++i) {
                x(i) = -6.0 + 0.2 * static_cast<double>(i);
                const double z = (x(i) - mu) / sigma;
                y(row, col, i) = 80.0 * std::exp(-0.5 * z * z) +
                                 2.0 * std::sin(7.3 * static_cast<double>(i));
            }
        }
    }

    struct Cube {
        NDArray<double, 3> par{{rows, cols, 3}};
        NDArray<double, 3> err{{rows, cols, 3}};
        NDArray<double, 2> chi2{{rows, cols}};
    };
    const auto fit_cube = [&](Minimizer minimizer) {
        Cube c;
        const auto model =
            make_model<aare::model::Gaussian>(minimizer, true, 500);
        aare::fit_3d(model, x.view(), y.view(), y_err.view(), c.par.view(),
                     c.err.view(), c.chi2.view(), 2);
        return c;
    };
    const auto ref = fit_cube(Minimizer::Migrad);

    for (const auto candidate :
         {Minimizer::Fumili, Minimizer::LevenbergMarquardt}) {
        INFO(name(candidate) << " vs Migrad");
        const auto res = fit_cube(candidate);
        for (ssize_t row = 0; row < rows; ++row) {
            for (ssize_t col = 0; col < cols; ++col) {
                for (ssize_t k = 0; k < 3; ++k) {
                    CHECK(res.par(row, col, k) ==
                          Approx(ref.par(row, col, k)).epsilon(1e-3));
                    CHECK(res.err(row, col, k) ==
                          Approx(ref.err(row, col, k)).epsilon(2e-2));
                }
                CHECK(res.chi2(row, col) ==
                      Approx(ref.chi2(row, col)).epsilon(1e-4));
            }
        }
    }
}
