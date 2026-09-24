// SPDX-License-Identifier: MPL-2.0
#include "aare/Fit.hpp"
#include "aare/FitModel.hpp"
#include "aare/Models.hpp"

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

// ----------------------------------------------------------------
// Rising S-curve (6 parameters), typical for threshold scans
// ----------------------------------------------------------------
static constexpr ssize_t N_SCURVE_POINTS = 100;

static void run_scurve_fit(benchmark::State &state, aare::Minimizer minimizer) {
    const std::vector<double> truth = {5.0, 0.1, 50.0, 4.0, 200.0, 0.5};
    const double noise_sigma = 0.02 * truth[4];

    aare::NDArray<double, 1> x({N_SCURVE_POINTS});
    aare::NDArray<double, 1> y({N_SCURVE_POINTS});
    std::mt19937 rng(SEED);
    std::normal_distribution<double> noise(0.0, noise_sigma);
    for (ssize_t i = 0; i < N_SCURVE_POINTS; ++i) {
        x[i] = static_cast<double>(i);
        y[i] = aare::model::RisingScurve::eval(x[i], truth) + noise(rng);
    }
    auto xv = x.view();
    auto yv = y.view();

    const aare::FitModel<aare::model::RisingScurve> model(0, 500, 0.5, false,
                                                          minimizer);

    aare::NDArray<double, 1> result;
    for (auto _ : state) {
        result = aare::fit_pixel<aare::model::RisingScurve>(model, xv, yv);
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

BENCHMARK(BM_FitGausMigrad)->DenseRange(0, 5)->Unit(benchmark::kMicrosecond);
BENCHMARK(BM_FitGausFumili)->DenseRange(0, 5)->Unit(benchmark::kMicrosecond);

BENCHMARK(BM_FitGausMigradHesse)
    ->DenseRange(0, 5)
    ->Unit(benchmark::kMicrosecond);
BENCHMARK(BM_FitGausFumiliErrors)
    ->DenseRange(0, 5)
    ->Unit(benchmark::kMicrosecond);

BENCHMARK(BM_FitScurveMigrad)->Unit(benchmark::kMicrosecond);
BENCHMARK(BM_FitScurveFumili)->Unit(benchmark::kMicrosecond);

BENCHMARK_MAIN();
