// SPDX-License-Identifier: MPL-2.0
#include "aare/Fit.hpp"
#include "aare/Chi2.hpp"
#include "aare/Models.hpp"
#include "aare/FitModel.hpp"

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
        {"Clean_signal",    1000.0,   50.0,  5.0, 0.02},
        {"Moderate_noise",  1000.0,   50.0,  5.0, 0.10},
        {"High_noise",      1000.0,   50.0,  5.0, 0.30},
        {"Narrow_peak",      500.0,   25.0,  1.0, 0.05},
        {"Wide_peak",        200.0,  100.0, 20.0, 0.05},
        {"Off_center_peak",  800.0,  -15.0,  3.0, 0.05},
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
        double clean = tc.true_A *
                       std::exp(-std::pow(d.x[i] - tc.true_mu, 2) /
                                (2.0 * std::pow(tc.true_sig, 2)));
        d.y[i] = clean + noise(rng);
        d.y_err[i] = noise_sigma;
    }
    return d;
}


static void report_accuracy(benchmark::State &state,
                            const TestCase &tc,
                            const aare::NDArray<double, 1> &result) {
    state.counters["dA"]   = result(0) - tc.true_A;
    state.counters["dMu"]  = result(1) - tc.true_mu;
    state.counters["dSig"] = result(2) - tc.true_sig;
}

// ----------
// Benchmarks
// ----------

// 1. lmcurve
static void BM_FitGausLm(benchmark::State &state) {
    const auto &tc = get_test_cases()[state.range(0)];
    auto data = generate_gaussian_data(tc);
    auto xv = data.x.view();
    auto yv = data.y.view();

    aare::NDArray<double, 1> result;
    for (auto _ : state) {
        result = aare::fit_gaus(xv, yv);
        benchmark::DoNotOptimize(result.data());
    }

    report_accuracy(state, tc, result);
    state.SetLabel(tc.name);
}

// 2. Minuit2, analytic gradient (no Hesse)
static void BM_FitGausMinuitGrad(benchmark::State &state) {
    const auto &tc = get_test_cases()[state.range(0)];
    auto data = generate_gaussian_data(tc);
    auto xv = data.x.view();
    auto yv = data.y.view();

    const auto model = aare::FitModel<aare::model::Gaussian>(/*strategy = */0,
                                                            /*max_calls = */500,             // increase for noisy signals
                                                            /*tolerance = */0.5, 
                                                            /*compute_errors = */false);

    aare::NDArray<double, 1> result;
    for (auto _ : state) {
        result = aare::fit_pixel<aare::model::Gaussian, aare::func::Chi2Gaussian>(model, xv, yv);
        benchmark::DoNotOptimize(result.data());
    }

    report_accuracy(state, tc, result);
    state.SetLabel(tc.name);
}

// 3. Minuit2, analytic gradient + Hesse
static void BM_FitGausMinuitGradHesse(benchmark::State &state) {
    const auto &tc = get_test_cases()[state.range(0)];
    auto data = generate_gaussian_data(tc);
    auto xv = data.x.view();
    auto yv = data.y.view();
    auto ev = data.y_err.view();

    const auto model = aare::FitModel<aare::model::Gaussian>(0, 500, 0.5, true); // compute_errors = true -> Runs Hesse and provides errors on fitted params

    aare::NDArray<double, 1> result;
    for (auto _ : state) {
        result = aare::fit_pixel<aare::model::Gaussian, aare::func::Chi2Gaussian>(model, xv, yv, ev);
        benchmark::DoNotOptimize(result.data());
    }

    // result has 6 elements: [A, mu, sig, err_A, err_mu, err_sig]
    report_accuracy(state, tc, result);

    // Also report Hesse uncertainties
    if (result.size() >= 6) {
        state.counters["errA"]   = result(3);
        state.counters["errMu"]  = result(4);
        state.counters["errSig"] = result(5);
    }
    state.SetLabel(tc.name);
}

BENCHMARK(BM_FitGausLm)
    ->DenseRange(0, 5)
    ->Unit(benchmark::kMicrosecond);

BENCHMARK(BM_FitGausMinuitGrad)
    ->DenseRange(0, 5)
    ->Unit(benchmark::kMicrosecond);

BENCHMARK(BM_FitGausMinuitGradHesse)
    ->DenseRange(0, 5)
    ->Unit(benchmark::kMicrosecond);

BENCHMARK_MAIN();