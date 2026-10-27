#include <benchmark/benchmark.h>

#include "aare/DataSetAggregation.hpp"
#include <filesystem>

/*
constexpr ssize_t size = 1024;
class JungfrauTestDataSet : public benchmark::Fixture {
  public:
    std::filesystem::path file_path =
        std::filesystem::path("/home/mazzol_a/Documents/temp_jungfrau/"
                              "CS_M749_2026-05-19_HGOG1G2_000000.dat");

    void SetUp(::benchmark::State &state) {}

    // void TearDown(::benchmark::State& state) {
    // }
};
*/

static void DatasetAggregation_MeanOverDataset(benchmark::State &st) {
    const std::filesystem::path file_path =
        "/home/mazzol_a/Documents/temp_jungfrau/"
        "BP_M749_2026-05-19_HGOG1G2_000000.dat";

    for (auto _ : st) {
        aare::NDArray<uint16_t, 3> res =
            aare::block_mean<uint16_t>(file_path, 220, 100, 16);

        benchmark::DoNotOptimize(res);
    }
}

static void DatasetAggregation_MeanCurrentSource(benchmark::State &st) {
    const std::filesystem::path file_path =
        "/home/mazzol_a/Documents/temp_jungfrau/"
        "CS_M749_2026-05-19_HGOG1G2_000000.dat";

    for (auto _ : st) {
        aare::NDArray<uint16_t, 3> res = aare::mean_current_source(
            file_path, 10, 97, 16,
            10 * 3 * 64); // 10 frames * 3 samples * 64 columns

        benchmark::DoNotOptimize(res);
    }
}

BENCHMARK(DatasetAggregation_MeanOverDataset);
BENCHMARK(DatasetAggregation_MeanCurrentSource);