// SPDX-License-Identifier: MPL-2.0
#include "aare/NDArray.hpp"
#include "aare/VarClusterFinder.hpp"
#include <benchmark/benchmark.h>
#include <random>

using namespace aare;

namespace {

constexpr ssize_t kRows = 512;
constexpr ssize_t kCols = 1024;
constexpr double kNoiseSigma = 1.0;
constexpr double kFixedThreshold = 10.0;

// Gaussian background with 3x3 photon clusters (100 in the centre, 20 in
// the eight neighbours) at random positions. Positions may overlap, which
// is fine for timing purposes as long as the same seed is used.
NDArray<double, 2> make_frame(int n_clusters, unsigned seed) {
    NDArray<double, 2> frame({kRows, kCols});
    std::mt19937 gen(seed);
    std::normal_distribution<double> noise(0.0, kNoiseSigma);
    for (auto &v : frame)
        v = noise(gen);

    std::uniform_int_distribution<ssize_t> row(1, kRows - 2);
    std::uniform_int_distribution<ssize_t> col(1, kCols - 2);
    for (int n = 0; n < n_clusters; ++n) {
        const auto r = row(gen);
        const auto c = col(gen);
        for (ssize_t dr = -1; dr <= 1; ++dr)
            for (ssize_t dc = -1; dc <= 1; ++dc)
                frame(r + dr, c + dc) += (dr == 0 && dc == 0) ? 100.0 : 20.0;
    }
    return frame;
}

// The finder may modify the frame in place, so each iteration starts from a
// pristine copy that is restored outside the timed region.
void run(benchmark::State &state, VarClusterFinder<double> &finder) {
    const auto pristine = make_frame(static_cast<int>(state.range(0)), 42);
    NDArray<double, 2> frame(pristine);
    size_t n_hits = 0;
    for (auto _ : state) {
        state.PauseTiming();
        std::copy(pristine.begin(), pristine.end(), frame.begin());
        state.ResumeTiming();
        finder.find_clusters(frame.view());
        auto hits = finder.steal_hits();
        n_hits = hits.size();
        benchmark::DoNotOptimize(hits.hits.data());
    }
    state.counters["hits"] = static_cast<double>(n_hits);
    state.counters["pixels/s"] =
        benchmark::Counter(static_cast<double>(kRows * kCols),
                           benchmark::Counter::kIsIterationInvariantRate);
}

} // namespace

static void BM_VarClusterFinder_FixedThreshold(benchmark::State &state) {
    VarClusterFinder<double> finder({kRows, kCols}, kFixedThreshold);
    run(state, finder);
}
BENCHMARK(BM_VarClusterFinder_FixedThreshold)
    ->Arg(100)
    ->Arg(1000)
    ->Arg(10000)
    ->Unit(benchmark::kMicrosecond);

static void BM_VarClusterFinder_NoiseMap(benchmark::State &state) {
    VarClusterFinder<double> finder({kRows, kCols}, kFixedThreshold);
    NDArray<double, 2> noise({kRows, kCols}, kNoiseSigma);
    finder.set_noise_map(noise.view());
    run(state, finder);
}
BENCHMARK(BM_VarClusterFinder_NoiseMap)
    ->Arg(100)
    ->Arg(1000)
    ->Arg(10000)
    ->Unit(benchmark::kMicrosecond);
