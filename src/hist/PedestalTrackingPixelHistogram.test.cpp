// SPDX-License-Identifier: MPL-2.0
#include "aare/hist/PedestalTrackingPixelHistogram.hpp"

#include "aare/File.hpp"
#include "aare/Frame.hpp"
#include "aare/NumpyFile.hpp"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

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
using aare::ssize_t;

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

TEST_CASE("Histogram-only fill crosses a pixel tile without changing pedestal",
          "[PedestalTrackingPixelHistogram]") {
    constexpr int rows = 1;
    constexpr int cols = 513;
    constexpr std::size_t pedestal_samples = 1000;

    const auto baseline = [](std::size_t col) {
        return static_cast<std::uint16_t>(100 + col % 17);
    };
    TemporaryHistogramFile file(
        rows, cols, 2,
        [baseline](std::size_t frame, std::size_t, std::size_t col) {
            return static_cast<std::uint16_t>(baseline(col) + frame + 1);
        });

    // Negative n_sigma exercises the complete histogram-only condition; zero
    // is covered by the ordered batch tests above.
    PedestalTrackingPixelHistogram histogram(rows, cols, 4, 0.0f, 4.0f, 1, 6,
                                             -1.0f);

    for (std::size_t seed = 0; seed < pedestal_samples; ++seed) {
        aare::NDArray<std::uint16_t, 2> frame({rows, cols});
        const int offset = seed % 2 == 0 ? -1 : 1;
        for (ssize_t col = 0; col < cols; ++col) {
            frame(0, col) = static_cast<std::uint16_t>(
                static_cast<int>(baseline(static_cast<std::size_t>(col))) +
                offset);
        }
        histogram.push_pedestal_no_update(frame.view());
    }
    histogram.update_mean();
    const auto mean_before = histogram.pedestal_mean();

    // One two-frame batch traverses the 512-pixel tile and its one-pixel tail.
    histogram.fill_from_file(file.path(), -1, false, 1, 2);

    const auto values = histogram.values();
    for (ssize_t col = 0; col < cols; ++col) {
        for (ssize_t bin = 0; bin < 4; ++bin) {
            CHECK(values(0, col, bin) == ((bin == 1 || bin == 2) ? 1 : 0));
        }
    }

    const auto mean_after = histogram.pedestal_mean();
    CHECK(
        std::equal(mean_before.begin(), mean_before.end(), mean_after.begin()));
}

TEST_CASE("Pedestal tracking threshold is strict and rejects zero sigma",
          "[PedestalTrackingPixelHistogram]") {
    constexpr std::size_t pedestal_samples = 1000;
    PedestalTrackingPixelHistogram histogram(1, 3, 8, -4.0f, 4.0f, 1, 4, 2.0f);

    // Pixels 0 and 1 have mean 100 and population sigma 1. Pixel 2 has the
    // same mean and zero sigma.
    for (std::size_t seed = 0; seed < pedestal_samples; ++seed) {
        aare::NDArray<std::uint16_t, 2> frame({1, 3});
        const auto noisy = static_cast<std::uint16_t>(seed % 2 == 0 ? 99 : 101);
        frame(0, 0) = noisy;
        frame(0, 1) = noisy;
        frame(0, 2) = 100;
        histogram.push_pedestal_no_update(frame.view());
    }
    histogram.update_mean();

    const auto mean_before = histogram.pedestal_mean();
    REQUIRE(mean_before(0, 0) == 100.0f);
    REQUIRE(mean_before(0, 1) == 100.0f);
    REQUIRE(mean_before(0, 2) == 100.0f);

    aare::NDArray<std::uint16_t, 2> frame({1, 3});
    frame(0, 0) = 102; // residual == 2 * sigma: excluded
    frame(0, 1) = 101; // residual < 2 * sigma: included
    frame(0, 2) = 101; // sigma == 0: excluded
    histogram.fill_async(std::move(frame));
    histogram.flush();

    const auto mean_after = histogram.pedestal_mean();
    CHECK(mean_after(0, 0) == 100.0f);
    CHECK(mean_after(0, 1) == static_cast<float>(100001.0 * (1.0 / 1000.0)));
    CHECK(mean_after(0, 2) == 100.0f);

    // Histogramming uses each residual before a possible EMA update.
    const auto values = histogram.values();
    for (ssize_t bin = 0; bin < 8; ++bin) {
        CHECK(values(0, 0, bin) == (bin == 6 ? 1 : 0));
        CHECK(values(0, 1, bin) == (bin == 5 ? 1 : 0));
        CHECK(values(0, 2, bin) == (bin == 5 ? 1 : 0));
    }
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

namespace {

constexpr int state_rows = 5;
constexpr int state_cols = 7;

// A baseline that differs from pixel to pixel, with an offset per frame
aare::NDArray<std::uint16_t, 2> state_test_frame(int offset) {
    aare::NDArray<std::uint16_t, 2> frame({state_rows, state_cols});
    for (ssize_t i = 0; i < frame.size(); ++i) {
        frame[i] = static_cast<std::uint16_t>(100 + i % 5 + offset);
    }
    return frame;
}

// 1000 pedestal frames with a noise of 2, then update_mean()
void seed_pedestal(PedestalTrackingPixelHistogram &histogram) {
    for (int seed = 0; seed < 1000; ++seed) {
        auto frame = state_test_frame(seed % 2 == 0 ? -2 : 2);
        histogram.push_pedestal_no_update(frame.view());
    }
    histogram.update_mean();
}

// Offsets inside and outside the gate of 2 sigma, so that only some frames
// update the pedestal
void fill(PedestalTrackingPixelHistogram &histogram, int first, int last) {
    const int offsets[] = {1, -3, 8, 0, 3, -1};
    for (int i = first; i < last; ++i) {
        histogram.fill_async(state_test_frame(offsets[i % 6]));
    }
}

template <typename A> bool same(const A &a, const A &b) {
    return a.shape() == b.shape() && std::equal(a.begin(), a.end(), b.begin());
}

void require_same(const PedestalTrackingPixelHistogram &actual,
                  const PedestalTrackingPixelHistogram &expected) {
    REQUIRE(same(actual.values(), expected.values()));
    REQUIRE(same(actual.pedestal_mean(), expected.pedestal_mean()));
    REQUIRE(same(actual.pedestal_std(), expected.pedestal_std()));
    REQUIRE(actual.n_sigma() == expected.n_sigma());
}

} // namespace

TEST_CASE("Pedestal tracking histogram restored from a state is filled "
          "further identically",
          "[PedestalTrackingPixelHistogram]") {
    aare::NDArray<bool, 2> mask({state_rows, state_cols}, false);
    mask(3, 4) = true;
    const aare::NDView<bool, 2> mask_view = mask.view();
    PedestalTrackingPixelHistogram original(state_rows, state_cols, 32, -16.0f,
                                            16.0f, 2, 16, 2.0f, mask_view);
    seed_pedestal(original);
    fill(original, 0, 9);
    // The pedestal has moved on since the noise was computed
    const auto state = original.state();
    REQUIRE(state.pedestal.n_samples == 1000);
    REQUIRE(state.pedestal.cur_samples == 1000);
    REQUIRE(state.n_sigma == 2.0);
    REQUIRE(same(state.histogram.values, original.values()));
    REQUIRE(state.mask.has_value());
    REQUIRE((*state.mask)(3, 4));

    // With another number of threads, whose row bands differ
    PedestalTrackingPixelHistogram restored(state, 3);
    // A histogram without mask that holds other data and settings
    PedestalTrackingPixelHistogram given(state_rows, state_cols, 32, -16.0f,
                                         16.0f, 1, 16, 5.0f);
    auto other_frame = state_test_frame(40);
    given.push_pedestal_no_update(other_frame.view());
    given.set_state(state);
    require_same(restored, original);
    require_same(given, original);

    fill(original, 9, 30);
    fill(restored, 9, 30);
    fill(given, 9, 30);
    REQUIRE_FALSE(same(original.values(), state.histogram.values));
    REQUIRE_FALSE(same(original.state().pedestal.sum, state.pedestal.sum));
    require_same(restored, original);
    require_same(given, original);
    // The masked pixel stayed empty
    const auto values = given.values();
    for (ssize_t bin = 0; bin < 32; ++bin) {
        REQUIRE(values(3, 4, bin) == 0);
    }
}

TEST_CASE("Pedestal tracking histogram takes a state while its pedestal is "
          "initializing and drops its mask",
          "[PedestalTrackingPixelHistogram]") {
    PedestalTrackingPixelHistogram original(state_rows, state_cols, 32, -16.0f,
                                            16.0f, 2);
    for (int seed = 0; seed < 400; ++seed) {
        auto frame = state_test_frame(seed % 2 == 0 ? -2 : 2);
        original.push_pedestal_no_update(frame.view());
    }
    const auto state = original.state();
    REQUIRE(state.pedestal.cur_samples == 400);
    REQUIRE_FALSE(state.mask.has_value());

    aare::NDArray<bool, 2> mask({state_rows, state_cols}, true);
    const aare::NDView<bool, 2> mask_view = mask.view();
    PedestalTrackingPixelHistogram given(state_rows, state_cols, 32, -16.0f,
                                         16.0f, 3, 16, 1.0f, mask_view);
    given.set_state(state);
    REQUIRE_FALSE(given.state().mask.has_value());

    // Both take the remaining pedestal frames and then fill alike
    for (auto *histogram : {&original, &given}) {
        for (int seed = 400; seed < 1000; ++seed) {
            auto frame = state_test_frame(seed % 2 == 0 ? -2 : 2);
            histogram->push_pedestal_no_update(frame.view());
        }
        histogram->update_mean();
        fill(*histogram, 0, 12);
    }
    require_same(given, original);
}

TEST_CASE("Pedestal tracking histogram rejects an invalid state and stays "
          "unchanged",
          "[PedestalTrackingPixelHistogram]") {
    using Catch::Matchers::ContainsSubstring;
    PedestalTrackingPixelHistogram histogram(state_rows, state_cols, 32, -16.0f,
                                             16.0f, 2, 16, 2.0f);
    seed_pedestal(histogram);
    fill(histogram, 0, 6);
    const auto valid = histogram.state();

    auto other_bins = valid;
    other_bins.histogram.values =
        aare::NDArray<std::uint16_t, 3>({state_rows, state_cols, 31}, 0);
    REQUIRE_THROWS_WITH(histogram.set_state(other_bins),
                        ContainsSubstring("expected (5, 7, 32)"));
    auto other_range = valid;
    other_range.histogram.xmax = 17.0;
    REQUIRE_THROWS_WITH(histogram.set_state(other_range),
                        ContainsSubstring("and [-16, 16)"));
    auto bad_pedestal = valid;
    bad_pedestal.pedestal.sum2 =
        aare::NDArray<double, 2>({state_rows, state_cols + 1}, 0.0);
    REQUIRE_THROWS_WITH(histogram.set_state(bad_pedestal),
                        ContainsSubstring("does not have the image shape"));
    auto bad_noise = valid;
    bad_noise.noise = aare::NDArray<double, 2>({state_rows + 1, state_cols});
    REQUIRE_THROWS_WITH(histogram.set_state(bad_noise),
                        ContainsSubstring("does not have the image shape"));
    auto bad_mask = valid;
    bad_mask.mask = aare::NDArray<bool, 2>({state_cols, state_rows}, false);
    REQUIRE_THROWS_WITH(histogram.set_state(bad_mask),
                        ContainsSubstring("does not have the image shape"));
    auto too_many = valid;
    too_many.pedestal.cur_samples = 1001;
    REQUIRE_THROWS_WITH(histogram.set_state(too_many),
                        ContainsSubstring("at most n_samples"));
    REQUIRE_THROWS_AS(PedestalTrackingPixelHistogram(
                          aare::PedestalTrackingPixelHistogramState{}),
                      std::runtime_error);

    PedestalTrackingPixelHistogram reference(valid);
    require_same(histogram, reference);
}
