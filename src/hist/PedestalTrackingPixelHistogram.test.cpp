// SPDX-License-Identifier: MPL-2.0
#include "aare/hist/PedestalTrackingPixelHistogram.hpp"

#include "aare/File.hpp"
#include "aare/Frame.hpp"
#include "aare/NumpyFile.hpp"

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <stdexcept>
#include <string>
#include <utility>

using aare::FileConfig;
using aare::Frame;
using aare::NumpyFile;
using aare::PedestalTrackingPixelHistogram;

namespace {

class TemporaryHistogramFile {
  public:
    using Generator =
        std::function<std::uint16_t(std::size_t, std::size_t, std::size_t)>;

    TemporaryHistogramFile(std::size_t rows = 2, std::size_t cols = 3,
                           std::size_t frames = 10, Generator generator = {}) {
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
                    image(row, col) =
                        generator ? generator(frame_index,
                                              static_cast<std::size_t>(row),
                                              static_cast<std::size_t>(col))
                                  : static_cast<std::uint16_t>(frame_index);
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

TEST_CASE("Tiled pedestal tracking matches chronological single-frame fills",
          "[PedestalTrackingPixelHistogram]") {
    constexpr int rows = 5;
    constexpr int cols = 257;
    constexpr std::size_t frames = 40;
    constexpr int bins = 32;
    constexpr float xmin = -16.0f;
    constexpr float xmax = 16.0f;

    const auto baseline = [](std::size_t row, std::size_t col) {
        return static_cast<std::uint16_t>(100 + (row + col) % 5);
    };
    const auto sample = [baseline](std::size_t frame, std::size_t row,
                                   std::size_t col) {
        const int offsets[] = {1, -1, 8, 0};
        return static_cast<std::uint16_t>(static_cast<int>(baseline(row, col)) +
                                          offsets[frame % 4]);
    };

    TemporaryHistogramFile file(rows, cols, frames, sample);
    PedestalTrackingPixelHistogram tiled(rows, cols, bins, xmin, xmax, 2, 16,
                                         2.0f);
    PedestalTrackingPixelHistogram chronological(rows, cols, bins, xmin, xmax,
                                                 2, 16, 2.0f);

    // Seed a non-zero cached standard deviation. Five rows split over two
    // workers make both row shards cross the 512-pixel tile boundary. Forty
    // data frames exercise chronological processing across a sizable batch.
    for (std::size_t seed = 0; seed < 1000; ++seed) {
        aare::NDArray<std::uint16_t, 2> frame({rows, cols});
        const int offset = seed % 2 == 0 ? -2 : 2;
        for (ssize_t row = 0; row < rows; ++row) {
            for (ssize_t col = 0; col < cols; ++col) {
                frame(row, col) = static_cast<std::uint16_t>(
                    static_cast<int>(baseline(row, col)) + offset);
            }
        }
        tiled.push_pedestal_no_update(frame.view());
        chronological.push_pedestal_no_update(frame.view());
    }
    tiled.update_mean();
    chronological.update_mean();

    // One forty-frame reader wave exercises tiled batch processing.
    tiled.fill_from_file(file.path(), -1, false, 2, 20);

    // The reference path establishes the same result one chronological frame
    // at a time, without any cross-frame traversal reordering.
    for (std::size_t frame_index = 0; frame_index < frames; ++frame_index) {
        aare::NDArray<std::uint16_t, 2> frame({rows, cols});
        for (ssize_t row = 0; row < rows; ++row) {
            for (ssize_t col = 0; col < cols; ++col) {
                frame(row, col) =
                    sample(frame_index, static_cast<std::size_t>(row),
                           static_cast<std::size_t>(col));
            }
        }
        chronological.fill_async(std::move(frame));
        chronological.flush();
    }

    const auto tiled_values = tiled.values();
    const auto chronological_values = chronological.values();
    REQUIRE(tiled_values.shape() == chronological_values.shape());
    CHECK(std::equal(tiled_values.begin(), tiled_values.end(),
                     chronological_values.begin()));

    const auto tiled_mean = tiled.pedestal_mean();
    const auto chronological_mean = chronological.pedestal_mean();
    REQUIRE(tiled_mean.shape() == chronological_mean.shape());
    CHECK(std::equal(tiled_mean.begin(), tiled_mean.end(),
                     chronological_mean.begin()));
}
