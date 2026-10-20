// SPDX-License-Identifier: MPL-2.0
#include "PixelHistogramCUDA_testimpl.hpp"
#include "aare/hist/PixelHistogram.hpp"
#include <catch2/catch_template_test_macros.hpp>
#include <cmath>
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

} // namespace

TEMPLATE_TEST_CASE("PixelHistogramCUDA matches PixelHistogram", "[cuda]",
                   uint8_t, uint16_t, uint32_t, uint64_t, float, double) {
    const size_t nrows = 5, ncols = 5, nframes = 100, nbins = 120;
    const float xmin = 3900.0f, xmax = 5100.0f;
    auto frames = make_synth(nrows, ncols, nframes);

    PixelHistogram<TestType> cpu(nrows, ncols, nbins, xmin, xmax);
    for (size_t f = 0; f < nframes; ++f)
        cpu.fill_async(NDArray<float, 2>(NDView<float, 2>(
            frames.data() + f * nrows * ncols,
            {static_cast<ssize_t>(nrows), static_cast<ssize_t>(ncols)})));

    auto gpu = test_cuda::run_gpu<TestType>(frames.view(), nbins, xmin, xmax,
                                            /*batch_size=*/8);
    REQUIRE((cpu.bin_edges() == gpu.edges));
    REQUIRE((cpu.bin_centers() == gpu.centers));
    REQUIRE((cpu.values() == gpu.values));
}