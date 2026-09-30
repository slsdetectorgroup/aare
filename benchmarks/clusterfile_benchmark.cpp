// SPDX-License-Identifier: MPL-2.0
#include "aare/ClusterFile.hpp"

#include <benchmark/benchmark.h>
#include <chrono>
#include <filesystem>
#include <string>
#include <system_error>

class ClusterFileFixture : public benchmark::Fixture {
    using Cluster = aare::Cluster<int32_t, 3, 3>;
    using File = aare::ClusterFile<Cluster>;
    using Vector = aare::ClusterVector<Cluster>;

    std::filesystem::path m_path;
    static constexpr int32_t frame_count = 256;
    static constexpr size_t chunk_size = 1000;

    static void observe(Vector &clusters) {
        benchmark::DoNotOptimize(clusters.data());
        benchmark::DoNotOptimize(clusters.size());
    }

  public:
    void SetUp(const benchmark::State &state) override {
        const auto stamp =
            std::chrono::steady_clock::now().time_since_epoch().count();
        m_path = std::filesystem::temp_directory_path() /
                 ("aare-cluster-benchmark-" + std::to_string(stamp) + ".clust");
        File writer(m_path, chunk_size, "w");
        Vector frame(0);
        frame.resize(state.range(0));
        for (int32_t number = 0; number < frame_count; ++number) {
            frame.set_frame_number(number);
            writer.write_frame(frame);
        }
    }

    void TearDown(const benchmark::State &) override {
        std::error_code error;
        std::filesystem::remove(m_path, error);
    }

    template <bool Frames, bool Iterate> void read(benchmark::State &state) {
        for (auto _ : state) {
            File reader(m_path, chunk_size);
            if constexpr (Iterate) {
                auto consume = [](auto range) {
                    for (auto &clusters : range) {
                        observe(clusters);
                    }
                };
                if constexpr (Frames) {
                    consume(reader.frames());
                } else {
                    consume(reader.chunks());
                }
            } else if constexpr (Frames) {
                Vector frame(0);
                while (reader.read_frame(frame)) {
                    observe(frame);
                }
            } else {
                while (true) {
                    auto chunk = reader.read_clusters(chunk_size);
                    if (chunk.empty()) {
                        break;
                    }
                    observe(chunk);
                }
            }
        }
        state.SetBytesProcessed(state.iterations() * frame_count *
                                state.range(0) * sizeof(Cluster));
    }
};

BENCHMARK_DEFINE_F(ClusterFileFixture, ReadFrames)(benchmark::State &state) {
    read<true, false>(state);
}
BENCHMARK_DEFINE_F(ClusterFileFixture, IterateFrames)(benchmark::State &state) {
    read<true, true>(state);
}
BENCHMARK_DEFINE_F(ClusterFileFixture, ReadChunks)(benchmark::State &state) {
    read<false, false>(state);
}
BENCHMARK_DEFINE_F(ClusterFileFixture, IterateChunks)(benchmark::State &state) {
    read<false, true>(state);
}

BENCHMARK_REGISTER_F(ClusterFileFixture, ReadFrames)->Arg(64)->Arg(1024);
BENCHMARK_REGISTER_F(ClusterFileFixture, IterateFrames)->Arg(64)->Arg(1024);
BENCHMARK_REGISTER_F(ClusterFileFixture, ReadChunks)->Arg(64)->Arg(1024);
BENCHMARK_REGISTER_F(ClusterFileFixture, IterateChunks)->Arg(64)->Arg(1024);
