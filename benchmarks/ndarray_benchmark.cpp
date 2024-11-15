#include <benchmark/benchmark.h>
#include "aare/NDArray.hpp"


using aare::NDArray;


NDArray<int,2> AddArrayWithOperator(NDArray<int,2> &a, NDArray<int,2> &b){
    return a+b;
}

NDArray<int,2> SubtractArrayWithOperator(NDArray<int,2> &a, NDArray<int,2> &b){
    return a-b;
}

NDArray<int,2> MultiplyArrayWithOperator(NDArray<int,2> &a, NDArray<int,2> &b){
    return a*b;
}

NDArray<int,2> DivideArrayWithOperator(NDArray<int,2> &a, NDArray<int,2> &b){
    return a/b;
}

NDArray<int,2> AddArrayWithIndex(NDArray<int,2> &a, NDArray<int,2> &b){
    NDArray<int,2> res(a.shape());
    for (uint32_t i = 0; i < a.size(); i++) {
        res(i) = a(i) + b(i);
    }
    return res;
}

constexpr ssize_t size = 1024;

static void BM_AddArrayWithOperator(benchmark::State& state) {
  // Perform setup here
    NDArray<int,2> a{{size,size},0};
    NDArray<int,2> b{{size,size},0};
  for (auto _ : state) {
    // This code gets timed
    NDArray<int,2> res = AddArrayWithOperator(a,b);
    benchmark::DoNotOptimize(res);
  }
}

static void BM_SubtractArrayWithOperator(benchmark::State& state) {
  // Perform setup here
    NDArray<int,2> a{{size,size},0};
    NDArray<int,2> b{{size,size},0};
  for (auto _ : state) {
    // This code gets timed
    NDArray<int,2> res = SubtractArrayWithOperator(a,b);
    benchmark::DoNotOptimize(res);
  }
}

static void BM_AddArrayWithIndex(benchmark::State& state) {
  // Perform setup here
    NDArray<int,2> a{{size,size},0};
    NDArray<int,2> b{{size,size},0};
  for (auto _ : state) {
    // This code gets timed
    NDArray<int,2> res = AddArrayWithIndex(a,b);
    benchmark::DoNotOptimize(res);
    
  }
}
// Register the function as a benchmark
BENCHMARK(BM_AddArrayWithOperator);
BENCHMARK(BM_AddArrayWithIndex);
BENCHMARK(BM_SubtractArrayWithOperator);
// Run the benchmark
BENCHMARK_MAIN();