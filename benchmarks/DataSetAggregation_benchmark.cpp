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
        "CS_M749_2026-05-19_HGOG1G2_000000.dat";

    for (auto _ : st) {
        aare::NDArray<uint16_t, 3> res =
            aare::mean_over_dataset<uint16_t>(file_path, 220, 10, 16);

        benchmark::DoNotOptimize(res);
    }
}

BENCHMARK(DatasetAggregation_MeanOverDataset);
