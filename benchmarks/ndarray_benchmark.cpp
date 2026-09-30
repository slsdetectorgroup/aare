// SPDX-License-Identifier: MPL-2.0
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

BENCHMARK_F(TwoArrays, ScalarMultiplyAddWithOperator)(benchmark::State &st) {
    for (auto _ : st) {
        // This code gets timed
        NDArray<int, 2> res = a * 2 + b;
        benchmark::DoNotOptimize(res);
    }
}
BENCHMARK_F(TwoArrays, ScalarMultiplyAddWithIndex)(benchmark::State &st) {
    for (auto _ : st) {
        // This code gets timed
        NDArray<int, 2> res(a.shape());
        for (uint32_t i = 0; i < a.size(); i++) {
            res(i) = a(i) * 2 + b(i);
        }
        benchmark::DoNotOptimize(res);
    }
}

BENCHMARK_F(TwoArrays, AddToExistingWithOperator)(benchmark::State &st) {
    NDArray<int, 2> res(a.shape());
    for (auto _ : st) {
        // This code gets timed
        res = a + b;
        benchmark::DoNotOptimize(res);
    }
}
BENCHMARK_F(TwoArrays, AddToExistingWithIndex)(benchmark::State &st) {
    NDArray<int, 2> res(a.shape());
    for (auto _ : st) {
        // This code gets timed
        for (uint32_t i = 0; i < a.size(); i++) {
            res(i) = a(i) + b(i);
        }
        benchmark::DoNotOptimize(res);
    }
}

// A store to uint8_t or a 64 bit integer can alias the members of NDArray.
// Compare with a loop over raw pointers to check that this does not block
// vectorization.
template <typename T> void InPlaceAddWithOperator(benchmark::State &st) {
    NDArray<T, 2> a({size, size}, 1);
    NDArray<T, 2> b({size, size}, 2);
    for (auto _ : st) {
        a += b;
        benchmark::DoNotOptimize(a);
    }
}
template <typename T> void InPlaceAddWithPointer(benchmark::State &st) {
    NDArray<T, 2> a({size, size}, 1);
    NDArray<T, 2> b({size, size}, 2);
    for (auto _ : st) {
        T *dst = a.data();
        const T *src = b.data();
        for (ssize_t i = 0, n = a.size(); i < n; ++i) {
            dst[i] += src[i];
        }
        benchmark::DoNotOptimize(a);
    }
}
template <typename T> void AddToExistingWithOperator(benchmark::State &st) {
    NDArray<T, 2> a({size, size}, 1);
    NDArray<T, 2> b({size, size}, 2);
    NDArray<T, 2> res({size, size}, 0);
    for (auto _ : st) {
        res = a + b;
        benchmark::DoNotOptimize(res);
    }
}
BENCHMARK_TEMPLATE(InPlaceAddWithOperator, uint8_t);
BENCHMARK_TEMPLATE(InPlaceAddWithPointer, uint8_t);
BENCHMARK_TEMPLATE(AddToExistingWithOperator, uint8_t);
BENCHMARK_TEMPLATE(InPlaceAddWithOperator, int64_t);
BENCHMARK_TEMPLATE(InPlaceAddWithPointer, int64_t);
BENCHMARK_TEMPLATE(AddToExistingWithOperator, int64_t);

BENCHMARK_MAIN();