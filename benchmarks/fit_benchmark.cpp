// SPDX-License-Identifier: MPL-2.0
#include "aare/Fit.hpp"
#include "aare/FitModel.hpp"
#include "aare/Models.hpp"
#ifdef AARE_MINUIT2
#include "aare/FitMinuit2.hpp"
#endif

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

// Backend tags: the built-in solver, and Minuit2 when compiled in.
struct Builtin {
    template <typename... Args>
    static aare::NDArray<double, 1> fit(Args &&...args) {
        return aare::fit_pixel<aare::model::Gaussian>(
            std::forward<Args>(args)...);
    }
};
#ifdef AARE_MINUIT2
struct Minuit2 {
    template <typename... Args>
    static aare::NDArray<double, 1> fit(Args &&...args) {
        return aare::minuit2::fit_pixel<aare::model::Gaussian>(
            std::forward<Args>(args)...);
    }
};
#endif

// ----------
// Benchmarks
// ----------

// Gaussian, unweighted, no errors
template <typename Backend>
static void BM_FitGaussian(benchmark::State &state) {
    const auto &tc = get_test_cases()[state.range(0)];
    auto data = generate_gaussian_data(tc);
    auto xv = data.x.view();
    auto yv = data.y.view();

    const auto model = aare::FitModel<aare::model::Gaussian>(
        /*strategy = */ 0,
        /*max_calls = */ 500, // increase for noisy signals
        /*tolerance = */ 0.5,
        /*compute_errors = */ false);

    aare::NDArray<double, 1> result;
    for (auto _ : state) {
        result = Backend::fit(model, xv, yv);
        benchmark::DoNotOptimize(result.data());
    }

    report_accuracy(state, tc, result);
    state.SetLabel(tc.name);
}

// Gaussian, weighted, with parameter errors
template <typename Backend>
static void BM_FitGaussianErrors(benchmark::State &state) {
    const auto &tc = get_test_cases()[state.range(0)];
    auto data = generate_gaussian_data(tc);
    auto xv = data.x.view();
    auto yv = data.y.view();
    auto ev = data.y_err.view();

    const auto model = aare::FitModel<aare::model::Gaussian>(0, 500, 0.5, true);

    aare::NDArray<double, 1> result;
    for (auto _ : state) {
        result = Backend::fit(model, xv, yv, ev);
        benchmark::DoNotOptimize(result.data());
    }

    // result has 7 elements: [A, mu, sig, err_A, err_mu, err_sig, chi2]
    report_accuracy(state, tc, result);
    if (result.size() >= 6) {
        state.counters["errA"] = result(3);
        state.counters["errMu"] = result(4);
        state.counters["errSig"] = result(5);
    }
    state.SetLabel(tc.name);
}

BENCHMARK_TEMPLATE(BM_FitGaussian, Builtin)
    ->DenseRange(0, 5)
    ->Unit(benchmark::kMicrosecond);

BENCHMARK_TEMPLATE(BM_FitGaussianErrors, Builtin)
    ->DenseRange(0, 5)
    ->Unit(benchmark::kMicrosecond);

#ifdef AARE_MINUIT2
BENCHMARK_TEMPLATE(BM_FitGaussian, Minuit2)
    ->DenseRange(0, 5)
    ->Unit(benchmark::kMicrosecond);

BENCHMARK_TEMPLATE(BM_FitGaussianErrors, Minuit2)
    ->DenseRange(0, 5)
    ->Unit(benchmark::kMicrosecond);
#endif

BENCHMARK_MAIN();
