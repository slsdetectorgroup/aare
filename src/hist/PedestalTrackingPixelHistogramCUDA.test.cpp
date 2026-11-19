// SPDX-License-Identifier: MPL-2.0
#include "PedestalTrackingPixelHistogramCUDA_testimpl.hpp"
#include "aare/FastPedestal.hpp"
#include "aare/hist/PedestalTrackingPixelHistogram.hpp"
#include "aare/hist/PixelHistogramImpl.hpp"
#include <algorithm>
#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <cmath>
#include <optional>
#include <random>
#include <vector>

using namespace aare;
using test_cuda::TrackingSetup;
using test_cuda::TrackingSnapshot;

namespace {

constexpr ssize_t NROWS = 9, NCOLS = 13; // non-square: catches a transpose

// uint16 frames of a pixel array with pedestals of 1000-3000 ADU and noise
// sigma 2-12 ADU. The data frames add what the tracking must handle: a slow
// drift of 20 ADU over the run, a 40 ADU step halfway in every fifth pixel,
// and photons of 30-400 ADU in 5% of the values, partly beyond xmax.
struct Frames {
    NDArray<uint16_t, 3> seed;
    NDArray<uint16_t, 3> data;
};

Frames make_frames(ssize_t n_seed, ssize_t n_data, unsigned rng_seed = 7) {
    std::mt19937 rng(rng_seed);
    std::uniform_real_distribution<double> base_d(1000.0, 3000.0);
    std::uniform_real_distribution<double> sigma_d(2.0, 12.0);
    std::uniform_real_distribution<double> unit(0.0, 1.0);
    std::uniform_real_distribution<double> photon_d(30.0, 400.0);
    std::normal_distribution<double> normal(0.0, 1.0);

    const ssize_t npix = NROWS * NCOLS;
    std::vector<double> base(npix), sigma(npix);
    for (ssize_t i = 0; i < npix; ++i) {
        base[i] = base_d(rng);
        sigma[i] = sigma_d(rng);
    }
    auto raw = [](double v) {
        return static_cast<uint16_t>(std::clamp(std::round(v), 0.0, 65535.0));
    };

    Frames f{NDArray<uint16_t, 3>({n_seed, NROWS, NCOLS}),
             NDArray<uint16_t, 3>({n_data, NROWS, NCOLS})};
    for (ssize_t t = 0; t < n_seed; ++t)
        for (ssize_t i = 0; i < npix; ++i)
            f.seed.data()[t * npix + i] = raw(base[i] + sigma[i] * normal(rng));
    for (ssize_t t = 0; t < n_data; ++t) {
        const double drift = 20.0 * static_cast<double>(t) / n_data;
        for (ssize_t i = 0; i < npix; ++i) {
            double v = base[i] + drift + sigma[i] * normal(rng);
            if (i % 5 == 0 && t >= n_data / 2)
                v += 40.0;
            if (unit(rng) < 0.05)
                v += photon_d(rng);
            f.data.data()[t * npix + i] = raw(v);
        }
    }
    return f;
}

NDView<uint16_t, 2> frame_of(NDView<uint16_t, 3> frames, ssize_t i) {
    return NDView<uint16_t, 2>(frames.data() +
                                   i * frames.shape(1) * frames.shape(2),
                               {frames.shape(1), frames.shape(2)});
}

// The sequence of run_tracking_gpu, on the CPU class.
std::vector<TrackingSnapshot>
run_tracking_cpu(NDView<uint16_t, 3> before, NDView<uint16_t, 3> seed,
                 NDView<uint16_t, 3> data, const TrackingSetup &setup,
                 const std::vector<size_t> &part_ends, bool update_between) {
    // The CPU class takes a mutable view.
    NDArray<bool, 2> mask_copy =
        setup.mask ? *setup.mask : NDArray<bool, 2>({NROWS, NCOLS}, false);
    std::optional<const NDView<bool, 2>> mask;
    if (setup.mask)
        mask.emplace(mask_copy.view());
    PedestalTrackingPixelHistogram cpu(
        NROWS, NCOLS, static_cast<int>(setup.nbins), setup.xmin, setup.xmax,
        /*n_threads=*/3, /*max_pending=*/16, setup.n_sigma, mask);

    auto fill = [&](NDView<uint16_t, 3> frames) {
        for (ssize_t i = 0; i < frames.shape(0); ++i)
            cpu.fill_async(NDArray<uint16_t, 2>(frame_of(frames, i)));
        cpu.flush();
    };
    fill(before);
    for (ssize_t i = 0; i < seed.shape(0); ++i)
        cpu.push_pedestal_no_update(frame_of(seed, i));
    cpu.update_mean();

    std::vector<TrackingSnapshot> snapshots;
    size_t begin = 0;
    for (size_t end : part_ends) {
        fill(data.sub_view(static_cast<ssize_t>(begin),
                           static_cast<ssize_t>(end)));
        snapshots.push_back(
            {cpu.values(), cpu.pedestal_mean(), cpu.pedestal_std()});
        if (update_between)
            cpu.update_mean();
        begin = end;
    }
    return snapshots;
}

void require_equal(const std::vector<TrackingSnapshot> &cpu,
                   const std::vector<TrackingSnapshot> &gpu) {
    REQUIRE(cpu.size() == gpu.size());
    for (size_t i = 0; i < cpu.size(); ++i) {
        CAPTURE(i);
        REQUIRE((cpu[i].values == gpu[i].values));
        REQUIRE((cpu[i].mean == gpu[i].mean));
        REQUIRE((cpu[i].std == gpu[i].std));
    }
}

NDArray<uint16_t, 3> no_frames() {
    return NDArray<uint16_t, 3>({0, NROWS, NCOLS});
}

} // namespace

TEST_CASE("PedestalTrackingPixelHistogramCUDA equals the CPU class", "[cuda]") {
    const bool window = GENERATE(true, false);
    const size_t batch_size = GENERATE(1, 37, 1024);
    const float n_sigma = GENERATE(1.0f, 3.0f);
    CAPTURE(window, batch_size, n_sigma);

    auto f = make_frames(1000, 300);
    auto before = no_frames();
    TrackingSetup setup;
    setup.n_sigma = n_sigma;
    setup.batch_size = batch_size;
    setup.window = window;
    // Three fills, sigma refreshed between them.
    const std::vector<size_t> parts{100, 101, 300};

    auto gpu = test_cuda::run_tracking_gpu(before.view(), f.seed.view(),
                                           f.data.view(), setup, parts, true);
    auto cpu = run_tracking_cpu(before.view(), f.seed.view(), f.data.view(),
                                setup, parts, true);
    require_equal(cpu, gpu);

    // The pedestal did move: the tracking ran.
    FastPedestal<float> ped(NROWS, NCOLS);
    for (ssize_t i = 0; i < 1000; ++i)
        ped.add_init_frame(frame_of(f.seed.view(), i));
    REQUIRE_FALSE((ped.mean() == gpu.back().mean));
}

TEST_CASE("PedestalTrackingPixelHistogramCUDA tracks residuals outside the "
          "histogram range",
          "[cuda]") {
    // A range narrower than the gate: values outside [xmin, xmax) but within
    // n_sigma * sigma are not counted, yet update the pedestal, as on the CPU.
    const bool window = GENERATE(true, false);
    CAPTURE(window);
    auto f = make_frames(1000, 200);
    auto before = no_frames();
    TrackingSetup setup;
    setup.nbins = 8;
    setup.xmin = -4.0f;
    setup.xmax = 4.0f;
    setup.n_sigma = 3.0f;
    setup.window = window;
    const std::vector<size_t> parts{200};
    require_equal(run_tracking_cpu(before.view(), f.seed.view(), f.data.view(),
                                   setup, parts, false),
                  test_cuda::run_tracking_gpu(before.view(), f.seed.view(),
                                              f.data.view(), setup, parts,
                                              false));
}

TEST_CASE("PedestalTrackingPixelHistogramCUDA with n_sigma = 0 only "
          "histograms",
          "[cuda]") {
    auto f = make_frames(1000, 200);
    auto before = no_frames();
    TrackingSetup setup;
    setup.n_sigma = 0.0f;
    const std::vector<size_t> parts{200};
    auto gpu = test_cuda::run_tracking_gpu(before.view(), f.seed.view(),
                                           f.data.view(), setup, parts, false);
    require_equal(run_tracking_cpu(before.view(), f.seed.view(), f.data.view(),
                                   setup, parts, false),
                  gpu);

    FastPedestal<float> ped(NROWS, NCOLS);
    for (ssize_t i = 0; i < 1000; ++i)
        ped.add_init_frame(frame_of(f.seed.view(), i));
    REQUIRE((ped.mean() == gpu.back().mean));
}

TEST_CASE("PedestalTrackingPixelHistogramCUDA skips masked pixels", "[cuda]") {
    const bool window = GENERATE(true, false);
    CAPTURE(window);
    auto f = make_frames(1000, 200);
    auto before = no_frames();
    NDArray<bool, 2> mask({NROWS, NCOLS}, false);
    mask(0, 0) = mask(4, 7) = mask(NROWS - 1, NCOLS - 1) = true;
    TrackingSetup setup;
    setup.mask = &mask;
    setup.window = window;
    const std::vector<size_t> parts{120, 200};
    auto gpu = test_cuda::run_tracking_gpu(before.view(), f.seed.view(),
                                           f.data.view(), setup, parts, true);
    require_equal(run_tracking_cpu(before.view(), f.seed.view(), f.data.view(),
                                   setup, parts, true),
                  gpu);
    for (size_t b = 0; b < setup.nbins; ++b)
        REQUIRE(gpu.back().values(4, 7, b) == 0);
}

TEST_CASE("PedestalTrackingPixelHistogramCUDA fills before the seed like the "
          "CPU class",
          "[cuda]") {
    // Before the seed the mean is 0: raw values are histogrammed. A range
    // over both raw values and residuals shows both.
    auto f = make_frames(1000, 100);
    NDArray<uint16_t, 3> before({50, NROWS, NCOLS});
    std::copy(f.data.data(), f.data.data() + before.size(), before.data());
    TrackingSetup setup;
    setup.nbins = 4100;
    setup.xmin = -100.0f;
    setup.xmax = 4000.0f;
    const std::vector<size_t> parts{100};
    require_equal(run_tracking_cpu(before.view(), f.seed.view(), f.data.view(),
                                   setup, parts, false),
                  test_cuda::run_tracking_gpu(before.view(), f.seed.view(),
                                              f.data.view(), setup, parts,
                                              false));
}

TEST_CASE("PedestalTrackingPixelHistogramCUDA with n_samples other than 1000",
          "[cuda]") {
    // The CPU class only has 1000 seed frames; this reference is its fill
    // loop, with FastPedestal and PixelHistogramImpl, for any n_samples.
    const uint32_t n_samples = 10;
    auto f = make_frames(n_samples, 200);
    auto before = no_frames();
    TrackingSetup setup;
    setup.n_samples = n_samples;
    setup.n_sigma = 2.0f;
    const std::vector<size_t> parts{200};
    auto gpu = test_cuda::run_tracking_gpu(before.view(), f.seed.view(),
                                           f.data.view(), setup, parts, false);

    FastPedestal<float> ped(NROWS, NCOLS, n_samples);
    for (uint32_t i = 0; i < n_samples; ++i)
        ped.add_init_frame(frame_of(f.seed.view(), i));
    const auto sigma = ped.std();
    PixelHistogramImpl<float, uint16_t> hist(
        NROWS, NCOLS, static_cast<int>(setup.nbins), setup.xmin, setup.xmax);
    for (ssize_t t = 0; t < f.data.shape(0); ++t) {
        const auto frame = frame_of(f.data.view(), t);
        for (ssize_t i = 0; i < NROWS * NCOLS; ++i) {
            const uint16_t raw = frame[i];
            const float val = static_cast<float>(raw) - ped.mean_unchecked(i);
            hist.fill_flat_unchecked(static_cast<size_t>(i), val);
            if (sigma[i] > 0.0f && std::abs(val) < setup.n_sigma * sigma[i])
                ped.push_ema_unchecked(static_cast<size_t>(i), raw);
        }
    }
    REQUIRE((hist.values() == gpu.back().values));
    REQUIRE((ped.mean() == gpu.back().mean));
    REQUIRE((sigma == gpu.back().std));
}

TEST_CASE("PedestalTrackingPixelHistogramCUDA rejects misuse", "[cuda]") {
    const auto r = test_cuda::tracking_errors();
    CHECK(r.update_mean_before_seed);
    CHECK(r.pedestal_mean_before_seed);
    CHECK(r.push_wrong_shape);
    CHECK(r.push_after_seed);
    CHECK(r.zero_n_samples);
    CHECK(r.mask_wrong_shape);
    CHECK(r.std_zero_before_update);
    CHECK(r.frames_after_seed == 3);
}
