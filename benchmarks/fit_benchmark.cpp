// SPDX-License-Identifier: MPL-2.0
#include "aare/Fit.hpp"
#include "aare/FitModel.hpp"
#include "aare/Models.hpp"

#include <algorithm>
#include <benchmark/benchmark.h>
#include <cmath>
#include <random>
#include <string>
#include <vector>

struct TestCase {
    std::string name;
    double true_A;
    double true_mu;
    double true_sig;
    double noise_frac;
};

static const std::vector<TestCase> &get_test_cases() {
    static const std::vector<TestCase> cases = {
        {"Clean_signal", 1000.0, 50.0, 5.0, 0.02},
        {"Moderate_noise", 1000.0, 50.0, 5.0, 0.10},
        {"High_noise", 1000.0, 50.0, 5.0, 0.30},
        {"Narrow_peak", 500.0, 25.0, 1.0, 0.05},
        {"Wide_peak", 200.0, 100.0, 20.0, 0.05},
        {"Off_center_peak", 800.0, -15.0, 3.0, 0.05},
    };
    return cases;
}

// ----------------------------------------------------------------
// Synthetic data generation (deterministic per test case)
// ----------------------------------------------------------------
static constexpr ssize_t N_POINTS = 100;
static constexpr unsigned SEED = 42;

struct GeneratedData {
    aare::NDArray<double, 1> x;
    aare::NDArray<double, 1> y;
    aare::NDArray<double, 1> y_err;

    GeneratedData() : x({N_POINTS}), y({N_POINTS}), y_err({N_POINTS}) {}
};

static GeneratedData generate_gaussian_data(const TestCase &tc) {
    GeneratedData d;

    double x_min = tc.true_mu - 5.0 * tc.true_sig;
    double x_max = tc.true_mu + 5.0 * tc.true_sig;
    double dx = (x_max - x_min) / (N_POINTS - 1);

    std::mt19937 rng(SEED);
    double noise_sigma = tc.noise_frac * tc.true_A;
    std::normal_distribution<double> noise(0.0, noise_sigma);

    for (ssize_t i = 0; i < N_POINTS; ++i) {
        d.x[i] = x_min + i * dx;
        double clean = tc.true_A * std::exp(-std::pow(d.x[i] - tc.true_mu, 2) /
                                            (2.0 * std::pow(tc.true_sig, 2)));
        d.y[i] = clean + noise(rng);
        d.y_err[i] = noise_sigma;
    }
    return d;
}

static void report_accuracy(benchmark::State &state, const TestCase &tc,
                            const aare::NDArray<double, 1> &result) {
    state.counters["dA"] = result(0) - tc.true_A;
    state.counters["dMu"] = result(1) - tc.true_mu;
    state.counters["dSig"] = result(2) - tc.true_sig;
}

// ----------
// Benchmarks
// ----------

static void run_gaussian_fit(benchmark::State &state, aare::Minimizer minimizer,
                             bool compute_errors) {
    const auto &tc = get_test_cases()[state.range(0)];
    auto data = generate_gaussian_data(tc);
    auto xv = data.x.view();
    auto yv = data.y.view();
    auto ev = data.y_err.view();

    const aare::FitModel<aare::model::Gaussian> model(
        /*strategy = */ 0,
        /*max_calls = */ 500, // increase for noisy signals
        /*tolerance = */ 0.5, compute_errors, minimizer);

    aare::NDArray<double, 1> result;
    for (auto _ : state) {
        result = compute_errors
                     ? aare::fit_pixel<aare::model::Gaussian>(model, xv, yv, ev)
                     : aare::fit_pixel<aare::model::Gaussian>(model, xv, yv);
        benchmark::DoNotOptimize(result.data());
    }

    report_accuracy(state, tc, result);

    // With errors the result is [A, mu, sig, err_A, err_mu, err_sig, chi2]
    if (compute_errors && result.size() >= 6) {
        state.counters["errA"] = result(3);
        state.counters["errMu"] = result(4);
        state.counters["errSig"] = result(5);
    }
    state.SetLabel(tc.name);
}

// Migrad, analytic gradient (no Hesse)
static void BM_FitGausMigrad(benchmark::State &state) {
    run_gaussian_fit(state, aare::Minimizer::Migrad, false);
}

// Migrad, analytic gradient + Hesse for the parameter errors
static void BM_FitGausMigradHesse(benchmark::State &state) {
    run_gaussian_fit(state, aare::Minimizer::Migrad, true);
}

// Fumili, analytic gradient and linearised Hessian
static void BM_FitGausFumili(benchmark::State &state) {
    run_gaussian_fit(state, aare::Minimizer::Fumili, false);
}

// Fumili, parameter errors from the linearised covariance
static void BM_FitGausFumiliErrors(benchmark::State &state) {
    run_gaussian_fit(state, aare::Minimizer::Fumili, true);
}

// Built-in Levenberg-Marquardt, analytic Jacobian
static void BM_FitGausLM(benchmark::State &state) {
    run_gaussian_fit(state, aare::Minimizer::LevenbergMarquardt, false);
}

// Built-in Levenberg-Marquardt, Gauss-Newton parameter errors
static void BM_FitGausLMErrors(benchmark::State &state) {
    run_gaussian_fit(state, aare::Minimizer::LevenbergMarquardt, true);
}

// Gaussian with sigma fixed: exercises the fixed-parameter path of the
// minimizers on the Moderate_noise case.
static void run_gaussian_fixed_sigma_fit(benchmark::State &state,
                                         aare::Minimizer minimizer) {
    const auto &tc = get_test_cases()[1];
    auto data = generate_gaussian_data(tc);
    auto xv = data.x.view();
    auto yv = data.y.view();

    aare::FitModel<aare::model::Gaussian> model(0, 500, 0.5, false, minimizer);
    model.FixParameter(2, tc.true_sig);

    aare::NDArray<double, 1> result;
    for (auto _ : state) {
        result = aare::fit_pixel<aare::model::Gaussian>(model, xv, yv);
        benchmark::DoNotOptimize(result.data());
    }
    report_accuracy(state, tc, result);
}

static void BM_FitGausFixedSigmaMigrad(benchmark::State &state) {
    run_gaussian_fixed_sigma_fit(state, aare::Minimizer::Migrad);
}
static void BM_FitGausFixedSigmaFumili(benchmark::State &state) {
    run_gaussian_fixed_sigma_fit(state, aare::Minimizer::Fumili);
}
static void BM_FitGausFixedSigmaLM(benchmark::State &state) {
    run_gaussian_fixed_sigma_fit(state, aare::Minimizer::LevenbergMarquardt);
}

// ----------------------------------------------------------------
// Rising S-curve (6 parameters), typical for threshold scans
// ----------------------------------------------------------------
static constexpr ssize_t N_SCURVE_POINTS = 100;

static void run_scurve_fit(benchmark::State &state, aare::Minimizer minimizer,
                           bool weighted = false) {
    const std::vector<double> truth = {5.0, 0.1, 50.0, 4.0, 200.0, 0.5};
    const double noise_sigma = 0.02 * truth[4];

    aare::NDArray<double, 1> x({N_SCURVE_POINTS});
    aare::NDArray<double, 1> y({N_SCURVE_POINTS});
    aare::NDArray<double, 1> y_err({N_SCURVE_POINTS}, noise_sigma);
    std::mt19937 rng(SEED);
    std::normal_distribution<double> noise(0.0, noise_sigma);
    for (ssize_t i = 0; i < N_SCURVE_POINTS; ++i) {
        x[i] = static_cast<double>(i);
        y[i] = aare::model::RisingScurve::eval(x[i], truth) + noise(rng);
    }
    auto xv = x.view();
    auto yv = y.view();
    auto ev = y_err.view();

    const aare::FitModel<aare::model::RisingScurve> model(0, 500, 0.5, false,
                                                          minimizer);

    aare::NDArray<double, 1> result;
    for (auto _ : state) {
        result =
            weighted
                ? aare::fit_pixel<aare::model::RisingScurve>(model, xv, yv, ev)
                : aare::fit_pixel<aare::model::RisingScurve>(model, xv, yv);
        benchmark::DoNotOptimize(result.data());
    }

    state.counters["dMu"] = result(2) - truth[2];
    state.counters["dSig"] = result(3) - truth[3];
    state.counters["dA"] = result(4) - truth[4];
    state.counters["chi2"] = result(6);
}

static void BM_FitScurveMigrad(benchmark::State &state) {
    run_scurve_fit(state, aare::Minimizer::Migrad);
}

static void BM_FitScurveFumili(benchmark::State &state) {
    run_scurve_fit(state, aare::Minimizer::Fumili);
}

static void BM_FitScurveLM(benchmark::State &state) {
    run_scurve_fit(state, aare::Minimizer::LevenbergMarquardt);
}

// Weighted fits take the per-point uncertainties into the residuals.
static void BM_FitScurveWeightedMigrad(benchmark::State &state) {
    run_scurve_fit(state, aare::Minimizer::Migrad, true);
}
static void BM_FitScurveWeightedFumili(benchmark::State &state) {
    run_scurve_fit(state, aare::Minimizer::Fumili, true);
}
static void BM_FitScurveWeightedLM(benchmark::State &state) {
    run_scurve_fit(state, aare::Minimizer::LevenbergMarquardt, true);
}

// ----------------------------------------------------------------
// Pol2 (3 parameters, cheap model): exposes the solver overhead per
// iteration rather than the model evaluation.
// ----------------------------------------------------------------
static void run_pol2_fit(benchmark::State &state, aare::Minimizer minimizer) {
    constexpr ssize_t n = 51;
    const std::vector<double> truth = {12.0, -0.8, 0.05};

    aare::NDArray<double, 1> x({n});
    aare::NDArray<double, 1> y({n});
    std::mt19937 rng(SEED);
    std::normal_distribution<double> noise(0.0, 2.0);
    for (ssize_t i = 0; i < n; ++i) {
        x[i] = static_cast<double>(i);
        y[i] = aare::model::Pol2::eval(x[i], truth) + noise(rng);
    }
    auto xv = x.view();
    auto yv = y.view();

    const aare::FitModel<aare::model::Pol2> model(0, 500, 0.5, false,
                                                  minimizer);

    aare::NDArray<double, 1> result;
    for (auto _ : state) {
        result = aare::fit_pixel<aare::model::Pol2>(model, xv, yv);
        benchmark::DoNotOptimize(result.data());
    }
    state.counters["dp2"] = result(2) - truth[2];
}

static void BM_FitPol2Migrad(benchmark::State &state) {
    run_pol2_fit(state, aare::Minimizer::Migrad);
}
static void BM_FitPol2Fumili(benchmark::State &state) {
    run_pol2_fit(state, aare::Minimizer::Fumili);
}
static void BM_FitPol2LM(benchmark::State &state) {
    run_pol2_fit(state, aare::Minimizer::LevenbergMarquardt);
}

// ----------------------------------------------------------------
// GaussianChargeSharingKb (8 parameters, two exponentials and two erfs per
// point): the most expensive model per evaluation.
// ----------------------------------------------------------------
static void run_charge_sharing_kb_fit(benchmark::State &state,
                                      aare::Minimizer minimizer) {
    constexpr ssize_t n = 200;
    const std::vector<double> truth = {20.0,   0.05, 110.0, 6.0,
                                       1500.0, 0.1,  1.1,   0.1};

    aare::NDArray<double, 1> x({n});
    aare::NDArray<double, 1> y({n});
    std::mt19937 rng(SEED);
    std::normal_distribution<double> noise(0.0, 1.0);
    for (ssize_t i = 0; i < n; ++i) {
        x[i] = static_cast<double>(i);
        const double clean =
            aare::model::GaussianChargeSharingKb::eval(x[i], truth);
        y[i] = clean + std::sqrt(std::max(clean, 1.0)) * noise(rng);
    }
    auto xv = x.view();
    auto yv = y.view();

    const aare::FitModel<aare::model::GaussianChargeSharingKb> model(
        0, 500, 0.5, false, minimizer);

    aare::NDArray<double, 1> result;
    for (auto _ : state) {
        result = aare::fit_pixel<aare::model::GaussianChargeSharingKb>(model,
                                                                       xv, yv);
        benchmark::DoNotOptimize(result.data());
    }
    state.counters["dMu"] = result(2) - truth[2];
    state.counters["dN"] = result(4) - truth[4];
    state.counters["chi2"] = result(8);
}

static void BM_FitChargeSharingKbMigrad(benchmark::State &state) {
    run_charge_sharing_kb_fit(state, aare::Minimizer::Migrad);
}
static void BM_FitChargeSharingKbFumili(benchmark::State &state) {
    run_charge_sharing_kb_fit(state, aare::Minimizer::Fumili);
}
static void BM_FitChargeSharingKbLM(benchmark::State &state) {
    run_charge_sharing_kb_fit(state, aare::Minimizer::LevenbergMarquardt);
}

// ----------------------------------------------------------------
// Data cube through fit_3d on one thread: exposes the per-pixel overhead
// of each minimizer on top of the minimisation itself.
// ----------------------------------------------------------------
static constexpr ssize_t CUBE_ROWS = 32;
static constexpr ssize_t CUBE_COLS = 32;

static void run_cube_fit(benchmark::State &state, aare::Minimizer minimizer,
                         bool compute_errors) {
    const auto &tc = get_test_cases()[1]; // Moderate_noise
    const double x_min = tc.true_mu - 5.0 * tc.true_sig;
    const double x_max = tc.true_mu + 5.0 * tc.true_sig;
    const double dx = (x_max - x_min) / (N_POINTS - 1);
    const double noise_sigma = tc.noise_frac * tc.true_A;

    aare::NDArray<double, 1> x({N_POINTS});
    aare::NDArray<double, 3> y({CUBE_ROWS, CUBE_COLS, N_POINTS});
    aare::NDArray<double, 3> y_err({CUBE_ROWS, CUBE_COLS, N_POINTS},
                                   noise_sigma);
    std::mt19937 rng(SEED);
    std::normal_distribution<double> noise(0.0, noise_sigma);
    for (ssize_t i = 0; i < N_POINTS; ++i)
        x[i] = x_min + i * dx;
    for (ssize_t row = 0; row < CUBE_ROWS; ++row) {
        for (ssize_t col = 0; col < CUBE_COLS; ++col) {
            for (ssize_t i = 0; i < N_POINTS; ++i) {
                const double clean =
                    tc.true_A * std::exp(-std::pow(x[i] - tc.true_mu, 2) /
                                         (2.0 * std::pow(tc.true_sig, 2)));
                y(row, col, i) = clean + noise(rng);
            }
        }
    }

    aare::NDArray<double, 3> par({CUBE_ROWS, CUBE_COLS, 3});
    aare::NDArray<double, 3> err({CUBE_ROWS, CUBE_COLS, 3});
    aare::NDArray<double, 2> chi2({CUBE_ROWS, CUBE_COLS});
    const aare::FitModel<aare::model::Gaussian> model(
        0, 500, 0.5, compute_errors, minimizer);

    for (auto _ : state) {
        if (compute_errors) {
            aare::fit_3d(model, x.view(), y.view(), y_err.view(), par.view(),
                         err.view(), chi2.view(), 1);
        } else {
            aare::fit_3d(model, x.view(), y.view(), aare::NDView<double, 3>{},
                         par.view(), aare::NDView<double, 3>{}, chi2.view(), 1);
        }
        benchmark::DoNotOptimize(par.data());
    }

    state.SetItemsProcessed(state.iterations() * CUBE_ROWS * CUBE_COLS);
    state.counters["failed"] =
        static_cast<double>(std::count(chi2.begin(), chi2.end(), 0.0));
}

static void BM_FitCubeMigrad(benchmark::State &state) {
    run_cube_fit(state, aare::Minimizer::Migrad, false);
}
static void BM_FitCubeFumili(benchmark::State &state) {
    run_cube_fit(state, aare::Minimizer::Fumili, false);
}
static void BM_FitCubeLM(benchmark::State &state) {
    run_cube_fit(state, aare::Minimizer::LevenbergMarquardt, false);
}
static void BM_FitCubeMigradErrors(benchmark::State &state) {
    run_cube_fit(state, aare::Minimizer::Migrad, true);
}
static void BM_FitCubeFumiliErrors(benchmark::State &state) {
    run_cube_fit(state, aare::Minimizer::Fumili, true);
}
static void BM_FitCubeLMErrors(benchmark::State &state) {
    run_cube_fit(state, aare::Minimizer::LevenbergMarquardt, true);
}

BENCHMARK(BM_FitGausMigrad)->DenseRange(0, 5)->Unit(benchmark::kMicrosecond);
BENCHMARK(BM_FitGausFumili)->DenseRange(0, 5)->Unit(benchmark::kMicrosecond);
BENCHMARK(BM_FitGausLM)->DenseRange(0, 5)->Unit(benchmark::kMicrosecond);

BENCHMARK(BM_FitGausMigradHesse)
    ->DenseRange(0, 5)
    ->Unit(benchmark::kMicrosecond);
BENCHMARK(BM_FitGausFumiliErrors)
    ->DenseRange(0, 5)
    ->Unit(benchmark::kMicrosecond);
BENCHMARK(BM_FitGausLMErrors)->DenseRange(0, 5)->Unit(benchmark::kMicrosecond);

BENCHMARK(BM_FitGausFixedSigmaMigrad)->Unit(benchmark::kMicrosecond);
BENCHMARK(BM_FitGausFixedSigmaFumili)->Unit(benchmark::kMicrosecond);
BENCHMARK(BM_FitGausFixedSigmaLM)->Unit(benchmark::kMicrosecond);

BENCHMARK(BM_FitScurveMigrad)->Unit(benchmark::kMicrosecond);
BENCHMARK(BM_FitScurveFumili)->Unit(benchmark::kMicrosecond);
BENCHMARK(BM_FitScurveLM)->Unit(benchmark::kMicrosecond);

BENCHMARK(BM_FitScurveWeightedMigrad)->Unit(benchmark::kMicrosecond);
BENCHMARK(BM_FitScurveWeightedFumili)->Unit(benchmark::kMicrosecond);
BENCHMARK(BM_FitScurveWeightedLM)->Unit(benchmark::kMicrosecond);

BENCHMARK(BM_FitPol2Migrad)->Unit(benchmark::kMicrosecond);
BENCHMARK(BM_FitPol2Fumili)->Unit(benchmark::kMicrosecond);
BENCHMARK(BM_FitPol2LM)->Unit(benchmark::kMicrosecond);

BENCHMARK(BM_FitChargeSharingKbMigrad)->Unit(benchmark::kMicrosecond);
BENCHMARK(BM_FitChargeSharingKbFumili)->Unit(benchmark::kMicrosecond);
BENCHMARK(BM_FitChargeSharingKbLM)->Unit(benchmark::kMicrosecond);

// fit_3d runs in worker threads, so measure wall time.
BENCHMARK(BM_FitCubeMigrad)->Unit(benchmark::kMillisecond)->UseRealTime();
BENCHMARK(BM_FitCubeFumili)->Unit(benchmark::kMillisecond)->UseRealTime();
BENCHMARK(BM_FitCubeLM)->Unit(benchmark::kMillisecond)->UseRealTime();
BENCHMARK(BM_FitCubeMigradErrors)->Unit(benchmark::kMillisecond)->UseRealTime();
BENCHMARK(BM_FitCubeFumiliErrors)->Unit(benchmark::kMillisecond)->UseRealTime();
BENCHMARK(BM_FitCubeLMErrors)->Unit(benchmark::kMillisecond)->UseRealTime();

BENCHMARK_MAIN();
