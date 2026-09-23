// SPDX-License-Identifier: MPL-2.0
#include "aare/NDArray.hpp"
#include "aare/NDView.hpp"
#include <benchmark/benchmark.h>

using aare::NDArray;
using aare::NDView;

static void BM_CreateNDView(benchmark::State &st) {
    NDArray<int, 2> arr{{1024, 1024}, 0};
    for (auto _ : st) {
        // This code gets timed
        auto res = arr.view();
        benchmark::DoNotOptimize(res);
    }
}
BENCHMARK(BM_CreateNDView);