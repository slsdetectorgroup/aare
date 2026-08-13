// SPDX-License-Identifier: MPL-2.0
#include "aare/ClusterFinder.hpp"
#include "aare/File.hpp"
#include "aare/Frame.hpp"
#include <benchmark/benchmark.h>

#include <algorithm>
#include <array>
#include <cstdint>
#include <limits>
#include <numeric>
#include <random>
#include <string>
#include <vector>
namespace {

using Cluster3x3 = aare::Cluster<int32_t, 3, 3>;
using Cluster5x5 = aare::Cluster<int32_t, 5, 5>;
using Cluster7x7 = aare::Cluster<int32_t, 7, 7>;

template <typename ClusterType>
using ClusterFinder = aare::ClusterFinder<ClusterType, uint16_t, double>;

template <typename ClusterType> class BenchmarkData {
  public:
    aare::Shape<2> image_shape;
    std::vector<aare::Frame> v_pedestal_frames, v_test_frames;
    size_t n_pedestal_frames, n_test_frames;
    BenchmarkData() {
        auto pedestal_filepath = std::getenv("PEDESTAL_FILE");
        auto data_filepath = std::getenv("DATA_FILE");
        auto pedestal_frames = std::getenv("N_PEDESTAL_FRAMES");
        auto test_frames = std::getenv("N_TEST_FRAMES");
        // if not initialized, take 1000 pedestal and 1000 test frames
        n_pedestal_frames =
            pedestal_frames ? std::stoul(pedestal_frames) : 1000;
        n_test_frames = test_frames ? std::stoul(test_frames) : 1000;
        if (!pedestal_filepath || !data_filepath) {
            throw std::runtime_error("PEDESTAL_FILE and DATA_FILE environment "
                                     "variables must be set");
        }
        aare::File pedestal_file(pedestal_filepath);
        aare::File data_file(data_filepath);
        pedestal_file.seek(0);
        try {
            v_pedestal_frames = pedestal_file.read_n(n_pedestal_frames);
        } catch (const std::exception &e) {
            throw std::runtime_error("Error reading pedestal " +
                                     std::to_string(n_pedestal_frames) +
                                     " frames: " + std::string(e.what()));
        }
        data_file.seek(0);
        try {
            v_test_frames = data_file.read_n(n_test_frames);
        } catch (const std::exception &e) {
            throw std::runtime_error("Error reading test " +
                                     std::to_string(n_test_frames) +
                                     " frames: " + std::string(e.what()));
        }
        std::vector<size_t> frame_shape = {
            static_cast<size_t>(v_test_frames[0].rows()),
            static_cast<size_t>(v_test_frames[0].cols())};
        image_shape = aare::make_shape<2>(frame_shape);
    }

    void initialize_finder(ClusterFinder<ClusterType> &finder) {
        for (aare::Frame &frame : v_pedestal_frames) {
            finder.push_pedestal_frame(frame.view<uint16_t>());
        }
    }
};

template <typename ClusterType>
void run_benchmark(benchmark::State &state, const char *label,
                   bool update_pedestal) {
    BenchmarkData<ClusterType> data{};
    ClusterFinder<ClusterType> cluster_finder(data.image_shape);
    data.initialize_finder(cluster_finder);
    std::size_t cluster_count = 0;
    for (auto _ : state) {
        cluster_count = 0;
        for (aare::Frame &frame : data.v_test_frames) {
            cluster_finder.find_clusters(frame.view<uint16_t>(), 0,
                                         update_pedestal);

            cluster_count += cluster_finder.steal_clusters(true).size();
        }
        benchmark::DoNotOptimize(cluster_count);
    }

    auto n_test_frames = data.n_test_frames;
    state.counters["test_frames"] = n_test_frames;
    state.counters["clusters_per_frame"] =
        static_cast<double>(cluster_count) / static_cast<double>(n_test_frames);
    state.SetItemsProcessed(static_cast<int64_t>(state.iterations()) *
                            static_cast<int64_t>(n_test_frames));
    state.SetLabel(label);
}
// Wrapper for the old function
void BM_ClusterFinder_3x3(benchmark::State &state) {
    run_benchmark<Cluster3x3>(state, "find_clusters 3x3", true);
}
void BM_ClusterFinder_5x5(benchmark::State &state) {
    run_benchmark<Cluster5x5>(state, "find_clusters 5x5", true);
}
void BM_ClusterFinder_7x7(benchmark::State &state) {
    run_benchmark<Cluster7x7>(state, "find_clusters 7x7", true);
}
} // namespace

BENCHMARK(BM_ClusterFinder_3x3)->Unit(benchmark::kMillisecond);
BENCHMARK(BM_ClusterFinder_5x5)->Unit(benchmark::kMillisecond);
BENCHMARK(BM_ClusterFinder_7x7)->Unit(benchmark::kMillisecond);