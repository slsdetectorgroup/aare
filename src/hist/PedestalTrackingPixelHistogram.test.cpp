// SPDX-License-Identifier: MPL-2.0
#include "aare/hist/PedestalTrackingPixelHistogram.hpp"

#include "aare/File.hpp"
#include "aare/Frame.hpp"
#include "aare/NumpyFile.hpp"

#include <catch2/catch_test_macros.hpp>

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <stdexcept>
#include <string>

using aare::FileConfig;
using aare::Frame;
using aare::NumpyFile;
using aare::PedestalTrackingPixelHistogram;

namespace {

class TemporaryHistogramFile {
  public:
    TemporaryHistogramFile(std::size_t rows = 2, std::size_t cols = 3,
                           std::size_t frames = 10) {
        const auto unique =
            std::chrono::steady_clock::now().time_since_epoch().count();
        path_ = std::filesystem::temp_directory_path() /
                ("aare-pedestal-hist-" + std::to_string(unique) + ".npy");

        FileConfig config;
        config.dtype = aare::Dtype::UINT16;
        config.rows = rows;
        config.cols = cols;
        NumpyFile file(path_, "w", config);
        for (std::size_t frame_index = 0; frame_index < frames; ++frame_index) {
            Frame frame(rows, cols, config.dtype);
            auto image = frame.view<std::uint16_t>();
            for (ssize_t row = 0; row < image.shape(0); ++row) {
                for (ssize_t col = 0; col < image.shape(1); ++col) {
                    image(row, col) = static_cast<std::uint16_t>(frame_index);
                }
            }
            file.write(frame);
        }
    }

    ~TemporaryHistogramFile() { std::filesystem::remove(path_); }

    TemporaryHistogramFile(const TemporaryHistogramFile &) = delete;
    TemporaryHistogramFile &operator=(const TemporaryHistogramFile &) = delete;

    const std::filesystem::path &path() const { return path_; }

  private:
    std::filesystem::path path_;
};

} // namespace

TEST_CASE("Pedestal tracking histogram fills ordered multi-reader batches",
          "[PedestalTrackingPixelHistogram]") {
    TemporaryHistogramFile file;
    PedestalTrackingPixelHistogram histogram(2, 3, 10, 0.0f, 10.0f, 2, 4, 0.0f);

    // Two reader workers claiming two frames each produces a full batch of
    // four followed by a partial batch of three.
    histogram.fill_from_file(file.path(), 7, false, 2, 2);
    const auto values = histogram.values();

    for (ssize_t row = 0; row < 2; ++row) {
        for (ssize_t col = 0; col < 3; ++col) {
            for (ssize_t bin = 0; bin < 10; ++bin) {
                CHECK(values(row, col, bin) == (bin < 7 ? 1 : 0));
            }
        }
    }
}

TEST_CASE("Pedestal tracking file fill handles limits and reader options",
          "[PedestalTrackingPixelHistogram]") {
    TemporaryHistogramFile file;
    PedestalTrackingPixelHistogram histogram(2, 3, 10, 0.0f, 10.0f, 2, 4, 0.0f);

    CHECK_NOTHROW(histogram.fill_from_file(file.path(), 0, false, 2, 2));
    CHECK_THROWS_AS(histogram.fill_from_file(file.path(), -2, false, 2, 2),
                    std::invalid_argument);
    CHECK_THROWS_AS(histogram.fill_from_file(file.path(), -1, false, 0, 2),
                    std::invalid_argument);
    CHECK_THROWS_AS(histogram.fill_from_file(file.path(), -1, false, 2, 0),
                    std::invalid_argument);

    // Preserve the previous API's clamp-at-EOF behaviour.
    histogram.fill_from_file(file.path(), 100, false, 2, 3);
    const auto values = histogram.values();
    for (ssize_t row = 0; row < 2; ++row) {
        for (ssize_t col = 0; col < 3; ++col) {
            for (ssize_t bin = 0; bin < 10; ++bin) {
                CHECK(values(row, col, bin) == 1);
            }
        }
    }
}

TEST_CASE("Pedestal tracking file fill validates frame metadata",
          "[PedestalTrackingPixelHistogram]") {
    TemporaryHistogramFile wrong_shape(3, 3, 1);
    PedestalTrackingPixelHistogram histogram(2, 3, 10, 0.0f, 10.0f, 1, 4, 0.0f);

    CHECK_THROWS_AS(
        histogram.fill_from_file(wrong_shape.path(), -1, false, 2, 1),
        std::invalid_argument);
}
