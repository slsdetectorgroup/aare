#include "aare/NDArray.hpp"
#include <benchmark/benchmark.h>

using aare::NDArray;

constexpr ssize_t size = 1024;
class TwoArrays : public benchmark::Fixture {
  public:
    NDArray<int, 2> a{{size, size}, 0};
    NDArray<int, 2> b{{size, size}, 0};
    void SetUp(::benchmark::State &state) {
        for (uint32_t i = 0; i < size; i++) {
            for (uint32_t j = 0; j < size; j++) {
                a(i, j) = i * j + 1;
                b(i, j) = i * j + 1;
            }
        }
    }

    // void TearDown(::benchmark::State& state) {
    // }
};

BENCHMARK_F(TwoArrays, AddWithOperator)(benchmark::State &st) {
    for (auto _ : st) {
        // This code gets timed
        NDArray<int, 2> res = a + b;
        benchmark::DoNotOptimize(res);
    }
}
BENCHMARK_F(TwoArrays, AddWithIndex)(benchmark::State &st) {
    for (auto _ : st) {
        // This code gets timed
        NDArray<int, 2> res(a.shape());
        for (uint32_t i = 0; i < a.size(); i++) {
            res(i) = a(i) + b(i);
        }
        benchmark::DoNotOptimize(res);
    }
}

BENCHMARK_F(TwoArrays, SubtractWithOperator)(benchmark::State &st) {
    for (auto _ : st) {
        // This code gets timed
        NDArray<int, 2> res = a - b;
        benchmark::DoNotOptimize(res);
    }
}
BENCHMARK_F(TwoArrays, SubtractWithIndex)(benchmark::State &st) {
    for (auto _ : st) {
        // This code gets timed
        NDArray<int, 2> res(a.shape());
        for (uint32_t i = 0; i < a.size(); i++) {
            res(i) = a(i) - b(i);
        }
        benchmark::DoNotOptimize(res);
    }
}

BENCHMARK_F(TwoArrays, MultiplyWithOperator)(benchmark::State &st) {
    for (auto _ : st) {
        // This code gets timed
        NDArray<int, 2> res = a * b;
        benchmark::DoNotOptimize(res);
    }
}
BENCHMARK_F(TwoArrays, MultiplyWithIndex)(benchmark::State &st) {
    for (auto _ : st) {
        // This code gets timed
        NDArray<int, 2> res(a.shape());
        for (uint32_t i = 0; i < a.size(); i++) {
            res(i) = a(i) * b(i);
        }
        benchmark::DoNotOptimize(res);
    }
}

BENCHMARK_F(TwoArrays, DivideWithOperator)(benchmark::State &st) {
    for (auto _ : st) {
        // This code gets timed
        NDArray<int, 2> res = a / b;
        benchmark::DoNotOptimize(res);
    }
}
BENCHMARK_F(TwoArrays, DivideWithIndex)(benchmark::State &st) {
    for (auto _ : st) {
        // This code gets timed
        NDArray<int, 2> res(a.shape());
        for (uint32_t i = 0; i < a.size(); i++) {
            res(i) = a(i) / b(i);
        }
        benchmark::DoNotOptimize(res);
    }
}

BENCHMARK_F(TwoArrays, FourAddWithOperator)(benchmark::State &st) {
    for (auto _ : st) {
        // This code gets timed
        NDArray<int, 2> res = a + b + a + b;
        benchmark::DoNotOptimize(res);
    }
}
BENCHMARK_F(TwoArrays, FourAddWithIndex)(benchmark::State &st) {
    for (auto _ : st) {
        // This code gets timed
        NDArray<int, 2> res(a.shape());
        for (uint32_t i = 0; i < a.size(); i++) {
            res(i) = a(i) + b(i) + a(i) + b(i);
        }
        benchmark::DoNotOptimize(res);
    }
}

BENCHMARK_F(TwoArrays, MultiplyAddDivideWithOperator)(benchmark::State &st) {
    for (auto _ : st) {
        // This code gets timed
        NDArray<int, 2> res = a * a + b / a;
        benchmark::DoNotOptimize(res);
    }
}
BENCHMARK_F(TwoArrays, MultiplyAddDivideWithIndex)(benchmark::State &st) {
    for (auto _ : st) {
        // This code gets timed
        NDArray<int, 2> res(a.shape());
        for (uint32_t i = 0; i < a.size(); i++) {
            res(i) = a(i) * a(i) + b(i) / a(i);
        }
        benchmark::DoNotOptimize(res);
    }
}

BENCHMARK_MAIN();