// SPDX-License-Identifier: MPL-2.0
#include "PixelHistogramCUDA_testimpl.hpp"
#include "aare/hist/PixelHistogram.hpp"
#include <catch2/catch_template_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <cmath>
#include <limits>
#include <random>
#include <vector>

using namespace aare;

namespace {

NDArray<float, 3> make_synth(size_t nrows, size_t ncols, size_t nframes) {
    std::mt19937 rng(12345);
    std::uniform_real_distribution<float> base_d(4000.0f, 5000.0f);
    std::uniform_real_distribution<float> sigma_d(5.0f, 15.0f);

    size_t npix = nrows * ncols;
    std::vector<float> base(npix);
    std::vector<std::normal_distribution<float>> noise;
    noise.reserve(npix);

    for (size_t i = 0; i < npix; ++i) {
        base[i] = base_d(rng);
        noise.emplace_back(0.0f, sigma_d(rng));
    }

    NDArray<float, 3> stack({static_cast<ssize_t>(nframes),
                             static_cast<ssize_t>(nrows),
                             static_cast<ssize_t>(ncols)});

    float *data_ = stack.data();

    for (size_t f = 0; f < nframes; ++f) {
        for (size_t i = 0; i < npix; ++i) {
            data_[f * npix + i] =
                std::round(base[i] + noise[i](rng)); // rounding is optional
        }
    }

    return stack;
}

template <typename T>
void fill_cpu(PixelHistogram<T> &cpu, NDView<const float, 3> frames) {
    const size_t nframes = frames.shape(0);
    const size_t nrows = frames.shape(1);
    const size_t ncols = frames.shape(2);
    for (size_t f = 0; f < nframes; ++f)
        cpu.fill_async(NDArray<float, 2>(NDView<const float, 2>(
            frames.data() + f * nrows * ncols,
            {static_cast<ssize_t>(nrows), static_cast<ssize_t>(ncols)})));
}

} // anonymous namespace

TEMPLATE_TEST_CASE("PixelHistogramCUDA matches PixelHistogram on noisy frames",
                   "[cuda]", uint8_t, uint16_t, uint32_t, uint64_t, float,
                   double) {
    // Non-square, so swapped rows and columns or a wrong stride show up.
    const size_t nrows = 7, ncols = 13, nframes = 100, nbins = 120;
    const float xmin = 3900.0f, xmax = 5100.0f;
    auto frames = make_synth(nrows, ncols, nframes);

    PixelHistogram<TestType> cpu(nrows, ncols, nbins, xmin, xmax,
                                 /*n_threads=*/8);
    fill_cpu(cpu, frames.view());

    auto gpu = test_cuda::run_gpu<TestType>(frames.view(), nbins, xmin, xmax,
                                            /*batch_size=*/8);
    REQUIRE((cpu.bin_edges() == gpu.edges));
    REQUIRE((cpu.bin_centers() == gpu.centers));
    REQUIRE((cpu.values() == gpu.values));
}

TEMPLATE_TEST_CASE(
    "PixelHistogramCUDA saturates at the storage maximum like PixelHistogram",
    "[cuda]", uint8_t, uint16_t) {
    const size_t cap = std::numeric_limits<TestType>::max();
    const size_t nrows = 1, ncols = 1, nframes = 2 * cap, nbins = 120;
    const float xmin = 3900.0f, xmax = 5100.0f;

    // Centre of bin 60: 4505.0f
    const size_t bin_idx = 60;
    const float value = xmin + (bin_idx + 0.5f) * (xmax - xmin) / nbins;

    NDArray<float, 3> frames({static_cast<ssize_t>(nframes),
                              static_cast<ssize_t>(nrows),
                              static_cast<ssize_t>(ncols)},
                             value);

    PixelHistogram<TestType> cpu(nrows, ncols, nbins, xmin, xmax);
    fill_cpu(cpu, frames.view());

    auto gpu = test_cuda::run_gpu<TestType>(frames.view(), nbins, xmin, xmax,
                                            /*batch_size=*/1024);

    REQUIRE((cpu.values() == gpu.values));

    auto gpu_count = gpu.values(0, 0, bin_idx);

    REQUIRE(static_cast<size_t>(gpu_count) == cap);
}

TEST_CASE("PixelHistogramCUDA keeps [xmin, xmax) and drops NaN and infinities",
          "[cuda]") {
    const float xmin = 3900.0f, xmax = 5100.0f;
    const float inf = std::numeric_limits<float>::infinity();
    // One pixel per case, the same value in every frame.
    const std::vector<float> cases = {
        xmin,                       // first bin
        std::nextafter(xmax, xmin), // last bin
        xmax,                       // dropped: the range is half-open
        std::nextafter(xmin, -inf), // dropped
        std::numeric_limits<float>::quiet_NaN(),
        inf,
        -inf,
    };
    const size_t ncols = cases.size(), nframes = 10, nbins = 120;

    NDArray<float, 3> frames(
        {static_cast<ssize_t>(nframes), 1, static_cast<ssize_t>(ncols)});
    for (size_t f = 0; f < nframes; ++f)
        for (size_t c = 0; c < ncols; ++c)
            frames(f, 0, c) = cases[c];

    // Pixel-major. Pixel (0,0) counts in the first bin, pixel (0,1) in the
    // last; all other pixels stay empty.
    NDArray<uint32_t, 3> expected(
        {1, static_cast<ssize_t>(ncols), static_cast<ssize_t>(nbins)}, 0);
    expected(0, 0, 0) = nframes;
    expected(0, 1, nbins - 1) = nframes;

    // Compared with the expected counts rather than with PixelHistogram:
    // the CPU class still counts NaN in bin 0.
    auto gpu = test_cuda::run_gpu<uint32_t>(frames.view(), nbins, xmin, xmax,
                                            /*batch_size=*/4);
    REQUIRE((gpu.values == expected));
}

TEST_CASE("PixelHistogramCUDA result does not depend on the batch size",
          "[cuda]") {
    // 1: one frame per batch, both slots reused many times. 7: partial last
    // batch. 25: divides nframes. 100: exactly one batch. 150: one batch
    // smaller than its slot.
    const size_t batch_size = GENERATE(1, 7, 25, 100, 150);
    CAPTURE(batch_size);

    const size_t nrows = 7, ncols = 13, nframes = 100, nbins = 120;
    const float xmin = 3900.0f, xmax = 5100.0f;
    auto frames = make_synth(nrows, ncols, nframes);

    PixelHistogram<uint32_t> cpu(nrows, ncols, nbins, xmin, xmax);
    fill_cpu(cpu, frames.view());

    auto gpu = test_cuda::run_gpu<uint32_t>(frames.view(), nbins, xmin, xmax,
                                            batch_size);
    REQUIRE((cpu.values() == gpu.values));
}

TEST_CASE("PixelHistogramCUDA bins uint16 frames on a float axis", "[cuda]") {
    // 1000 bins over 1200 ADU: bins 1.2 wide, so an integral axis (scale
    // 1000 / 1200 == 0 in integers) would put everything in bin 0.
    const size_t nrows = 7, ncols = 13, nframes = 100, nbins = 1000;
    const float xmin = 3900.0f, xmax = 5100.0f;
    auto frames = make_synth(nrows, ncols, nframes); // integer-valued floats

    NDArray<uint16_t, 3> raw(frames.shape());
    for (ssize_t i = 0; i < frames.size(); ++i)
        raw.data()[i] = static_cast<uint16_t>(frames.data()[i]);

    // The CPU reference gets the same values as float.
    PixelHistogram<uint32_t> cpu(nrows, ncols, nbins, xmin, xmax);
    fill_cpu(cpu, frames.view());

    auto gpu = test_cuda::run_gpu<uint32_t>(raw.view(), nbins, xmin, xmax,
                                            /*batch_size=*/8);
    REQUIRE((cpu.bin_edges() == gpu.edges));
    REQUIRE((cpu.values() == gpu.values));
}

TEST_CASE("PixelHistogramCUDA accumulates over several fills with values() "
          "in between",
          "[cuda]") {
    const size_t nrows = 7, ncols = 13, nframes = 100, nbins = 120;
    const float xmin = 3900.0f, xmax = 5100.0f;
    auto frames = make_synth(nrows, ncols, nframes);

    // With batch_size 8: part 1 ends in a partial batch, part 2 is a single
    // frame, part 3 fills the rest after two values() calls.
    const std::vector<size_t> ends = {30, 31, nframes};
    auto snapshots = test_cuda::run_gpu_in_parts(frames.view(), nbins, xmin,
                                                 xmax, /*batch_size=*/8, ends);
    REQUIRE(snapshots.size() == ends.size());

    PixelHistogram<uint32_t> cpu(nrows, ncols, nbins, xmin, xmax);
    size_t begin = 0;
    for (size_t i = 0; i < ends.size(); ++i) {
        CAPTURE(ends[i]);
        fill_cpu(cpu, frames.view().sub_view(static_cast<ssize_t>(begin),
                                             static_cast<ssize_t>(ends[i])));
        REQUIRE((cpu.values() == snapshots[i]));
        begin = ends[i];
    }
}

TEST_CASE("PixelHistogramCUDA values_pixel matches the pixel's slice of "
          "values()",
          "[cuda]") {
    // 2000 frames push peak bins past 255, so the uint8_t cap in values_pixel
    // is exercised next to uncapped tail bins (saw_cap checks it happens).
    const size_t nrows = 7, ncols = 13, nframes = 2000, nbins = 120;
    const float xmin = 3900.0f, xmax = 5100.0f;
    auto frames = make_synth(nrows, ncols, nframes);

    const std::vector<std::pair<size_t, size_t>> pixels = {
        {0, 0}, {0, ncols - 1}, {nrows - 1, 0}, {nrows - 1, ncols - 1}, {3, 6}};
    auto gpu = test_cuda::run_gpu_values_pixel(frames.view(), nbins, xmin, xmax,
                                               /*batch_size=*/64, pixels);
    REQUIRE(gpu.pixels.size() == pixels.size());

    bool saw_cap = false;
    for (size_t i = 0; i < pixels.size(); ++i) {
        const auto [row, col] = pixels[i];
        CAPTURE(row, col);
        NDArray<uint8_t, 1> slice({static_cast<ssize_t>(nbins)});
        for (size_t b = 0; b < nbins; ++b) {
            slice(b) = gpu.values(row, col, b);
            saw_cap = saw_cap || slice(b) == 255;
        }
        REQUIRE((gpu.pixels[i] == slice));
    }
    REQUIRE(saw_cap);

    REQUIRE(gpu.row_out_of_range_throws);
    REQUIRE(gpu.col_out_of_range_throws);
}

TEST_CASE("PixelHistogramCUDA values() transposes and caps a known "
          "accumulator pattern",
          "[cuda]") {
    REQUIRE_NOTHROW(test_cuda::check_known_pattern());
}