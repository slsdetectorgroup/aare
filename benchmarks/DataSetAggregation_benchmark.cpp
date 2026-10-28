#include <benchmark/benchmark.h>

#include "aare/DataSetAggregation.hpp"
#include <filesystem>

static void DatasetAggregation_MeanOverDataset(benchmark::State &st) {
    const std::filesystem::path file_path =
        "/mnt/sls_det_storage/jungfrau_calib/data/Module_749_Calib/"
        "BP_M749_2026-05-19_HGOG1G2_000000.dat";

    for (auto _ : st) {
        aare::NDArray<uint16_t, 3> res =
            aare::block_mean<uint16_t>(file_path, 220, 100);

        benchmark::DoNotOptimize(res);
    }
}

static void DatasetAggregation_MeanCurrentSource(benchmark::State &st) {
    const std::filesystem::path file_path =
        "/mnt/sls_det_storage/jungfrau_calib/data/Module_749_Calib/"
        "CS_M749_2026-05-19_HGOG1G2_000000.dat";

    for (auto _ : st) {
        aare::NDArray<uint16_t, 3> res = aare::mean_current_source(
            file_path, 10, 97, 12, 8,
            10 * 3 * 64); // 10 frames * 3 samples * 64 columns

        benchmark::DoNotOptimize(res);
    }
}

BENCHMARK(DatasetAggregation_MeanOverDataset);
BENCHMARK(DatasetAggregation_MeanCurrentSource);