// SPDX-License-Identifier: MPL-2.0
#include "aare/GapPixels.hpp"
#include "aare/RawFile.hpp"

#include <benchmark/benchmark.h>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <nlohmann/json.hpp>
#include <optional>
#include <string>
#include <system_error>
#include <vector>

namespace {

/// One Jungfrau module, 512x1024 pixels at 16 bit, with every frame in a
/// single data file written to the temporary directory.
class RawFileFixture : public benchmark::Fixture {
  protected:
    static constexpr size_t rows = 512;
    static constexpr size_t cols = 1024;
    static constexpr size_t frame_count = 32;
    std::filesystem::path m_directory;

  public:
    void SetUp(const benchmark::State &) override {
        const auto stamp =
            std::chrono::steady_clock::now().time_since_epoch().count();
        m_directory = std::filesystem::temp_directory_path() /
                      ("aare-rawfile-benchmark-" + std::to_string(stamp));
        std::filesystem::create_directory(m_directory);
        const nlohmann::json metadata = {
            {"Version", 7.2},
            {"Detector Type", "Jungfrau"},
            {"Timing Mode", "auto"},
            {"Geometry", {{"x", 1}, {"y", 1}}},
            {"Image Size in bytes", rows * cols * sizeof(uint16_t)},
            {"Pixels", {{"x", cols}, {"y", rows}}},
            {"Max Frames Per File", frame_count},
            {"Total Frames", frame_count},
            {"Frames in File", frame_count},
            {"Frame Padding", 1},
            {"Frame Discard Policy", "nodiscard"}};
        std::ofstream(master()) << metadata;

        std::vector<uint16_t> pixels(rows * cols);
        for (size_t i = 0; i < pixels.size(); ++i) {
            pixels[i] = static_cast<uint16_t>(i % 1000 + 1);
        }
        std::ofstream data(m_directory / "run_d0_f0_0.raw", std::ios::binary);
        for (size_t frame = 0; frame < frame_count; ++frame) {
            aare::DetectorHeader header{};
            header.frameNumber = frame + 1;
            data.write(reinterpret_cast<const char *>(&header), sizeof(header));
            data.write(
                reinterpret_cast<const char *>(pixels.data()),
                static_cast<std::streamsize>(pixels.size() * sizeof(uint16_t)));
        }
    }

    void TearDown(const benchmark::State &) override {
        std::error_code error;
        std::filesystem::remove_all(m_directory, error);
    }

    std::filesystem::path master() const {
        return m_directory / "run_master_0.json";
    }

    /// Read every frame of the file into one buffer per iteration
    void read_all(benchmark::State &state,
                  std::optional<aare::GapPixels> gaps) const {
        aare::RawFile file(master(), "r", gaps);
        std::vector<std::byte> buffer(file.bytes_per_frame() * frame_count);
        for (auto _ : state) {
            file.seek(0);
            file.read_into(buffer.data(), frame_count);
            benchmark::DoNotOptimize(buffer.data());
            benchmark::ClobberMemory();
        }
        state.SetItemsProcessed(static_cast<int64_t>(state.iterations()) *
                                static_cast<int64_t>(frame_count));
        state.SetBytesProcessed(static_cast<int64_t>(state.iterations()) *
                                static_cast<int64_t>(buffer.size()));
    }
};

BENCHMARK_DEFINE_F(RawFileFixture, NoGaps)(benchmark::State &state) {
    read_all(state, std::nullopt);
}

BENCHMARK_DEFINE_F(RawFileFixture, GapsFillValue)(benchmark::State &state) {
    read_all(state, aare::GapPixels{});
}

BENCHMARK_DEFINE_F(RawFileFixture, GapsSplitCounts)(benchmark::State &state) {
    aare::GapPixels gaps;
    gaps.split_counts = true;
    gaps.seed = 1;
    read_all(state, gaps);
}

BENCHMARK_REGISTER_F(RawFileFixture, NoGaps)->Unit(benchmark::kMillisecond);
BENCHMARK_REGISTER_F(RawFileFixture, GapsFillValue)
    ->Unit(benchmark::kMillisecond);
BENCHMARK_REGISTER_F(RawFileFixture, GapsSplitCounts)
    ->Unit(benchmark::kMillisecond);

} // namespace
