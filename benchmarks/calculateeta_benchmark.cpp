#include "aare/CalculateEta.hpp"
#include "aare/ClusterFile.hpp"
#include <benchmark/benchmark.h>

using namespace aare;

class ClusterFixture : public benchmark::Fixture {
  public:
    Cluster<int, 2, 2> cluster_2x2{};
    Cluster<int, 3, 3> cluster_3x3{};

  private:
    using benchmark::Fixture::SetUp;

    void SetUp([[maybe_unused]] const benchmark::State &state) override {
        int temp_data[4] = {1, 2, 3, 1};
        std::copy(std::begin(temp_data), std::end(temp_data),
                  std::begin(cluster_2x2.data));

        cluster_2x2.x = 0;
        cluster_2x2.y = 0;

        int temp_data2[9] = {1, 2, 3, 1, 3, 4, 5, 1, 20};
        std::copy(std::begin(temp_data2), std::end(temp_data2),
                  std::begin(cluster_3x3.data));

        cluster_3x3.x = 0;
        cluster_3x3.y = 0;
    }

    // void TearDown(::benchmark::State& state) {
    // }
};

BENCHMARK_F(ClusterFixture, Calculate2x2Eta)(benchmark::State &st) {
    for (auto _ : st) {
        // This code gets timed
        Eta2 eta = calculate_eta2(cluster_2x2);
        benchmark::DoNotOptimize(eta);
    }
}

// almost takes double the time
BENCHMARK_F(ClusterFixture, CalculateGeneralEtaFor2x2Cluster)
(benchmark::State &st) {
    for (auto _ : st) {
        // This code gets timed
        Eta2 eta = calculate_eta2<int, 2, 2>(cluster_2x2);
        benchmark::DoNotOptimize(eta);
    }
}

BENCHMARK_F(ClusterFixture, Calculate3x3Eta)(benchmark::State &st) {
    for (auto _ : st) {
        // This code gets timed
        Eta2 eta = calculate_eta2(cluster_3x3);
        benchmark::DoNotOptimize(eta);
    }
}

// almost takes double the time
BENCHMARK_F(ClusterFixture, CalculateGeneralEtaFor3x3Cluster)
(benchmark::State &st) {
    for (auto _ : st) {
        // This code gets timed
        Eta2 eta = calculate_eta2<int, 3, 3>(cluster_3x3);
        benchmark::DoNotOptimize(eta);
    }
}
// BENCHMARK_MAIN();