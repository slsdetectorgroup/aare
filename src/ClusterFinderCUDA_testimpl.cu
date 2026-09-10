// SPDX-License-Identifier: MPL-2.0
#include "ClusterFinderCUDA_testimpl.hpp"
#include "aare/ClusterFinderCUDA.hpp"
#include "aare/clusterfinder_algo.cuh"
#include <algorithm>
#include <random>

namespace aare::test_cuda {
namespace {

struct Synth {
    int nrows, ncols, n_ped, n_sig;
    std::vector<uint16_t> ped; // n_ped frames
    std::vector<uint16_t> sig; // n_sig frames with photons
};

/// Deterministic pedestal + signal frames: per-pixel baseline in [4000, 5000),
/// Gaussian noise of rms 8, and photons at random positions plus all four
/// corners so the halo and bounds paths are exercised.
Synth make_synth(int nrows, int ncols, int n_ped, int n_sig) {
    std::mt19937 rng(12345);
    std::uniform_real_distribution<double> base_d(4000.0, 5000.0);
    std::normal_distribution<double> noise(0.0, 8.0);
    std::uniform_int_distribution<int> row_d(0, nrows - 1), col_d(0, ncols - 1);
    std::uniform_real_distribution<double> amp(150.0, 400.0);

    const size_t npx = static_cast<size_t>(nrows) * ncols;
    std::vector<double> base(npx);
    for (auto &b : base)
        b = base_d(rng);

    auto to_u16 = [](double v) {
        return static_cast<uint16_t>(std::clamp(std::lround(v), 0L, 65535L));
    };

    Synth s{nrows, ncols, n_ped, n_sig, {}, {}};
    s.ped.resize(static_cast<size_t>(n_ped) * npx);
    for (int f = 0; f < n_ped; ++f)
        for (size_t i = 0; i < npx; ++i)
            s.ped[f * npx + i] = to_u16(base[i] + noise(rng));

    s.sig.resize(static_cast<size_t>(n_sig) * npx);
    std::vector<double> frame(npx);
    for (int f = 0; f < n_sig; ++f) {
        for (size_t i = 0; i < npx; ++i)
            frame[i] = base[i] + noise(rng);
        for (int k = 0; k < 40; ++k) {
            const int r = row_d(rng), c = col_d(rng);
            frame[r * ncols + c] += amp(rng);
            if (c + 1 < ncols)
                frame[r * ncols + c + 1] += 0.3 * amp(rng);
        }
        for (int r : {0, nrows - 1})
            for (int c : {0, ncols - 1})
                frame[r * ncols + c] += 300.0;
        for (size_t i = 0; i < npx; ++i)
            s.sig[f * npx + i] = to_u16(frame[i]);
    }
    return s;
}

template <int N>
ManualVsDriver<Cluster<int32_t, N, N>> run(int nrows, int ncols, int n_ped,
                                           int n_frames) {
    using C = Cluster<int32_t, N, N>;
    using Algo = cuda::FixedWindow<C, uint16_t>;

    const size_t npx = static_cast<size_t>(nrows) * ncols;
    const uint32_t max_clusters = 4096;
    const float nSigma = 5.0f;
    const Synth s = make_synth(nrows, ncols, n_ped, n_frames);
    const Shape<2> shape{static_cast<ssize_t>(nrows),
                         static_cast<ssize_t>(ncols)};

    ManualVsDriver<C> out;
    out.manual.resize(n_frames);
    out.driver.resize(n_frames);

    // ---- Route 1: caller-owned buffers, nothing from the driver ----------
    {
        Pedestal<double> ped(nrows, ncols);
        for (int f = 0; f < n_ped; ++f)
            ped.push(NDView<uint16_t, 2>(
                const_cast<uint16_t *>(s.ped.data() + f * npx), shape));

        std::vector<float> offset;
        const auto img = Algo::prepare_pedestal(ped, offset);

        // Raw cudaMalloc on purpose: this route must not touch
        // PedestalBuffers or StreamContext, only the DeviceState contract.
        float *d_mean = nullptr, *d_sum = nullptr, *d_sum2 = nullptr,
              *d_off = nullptr;
        uint16_t *d_frame = nullptr;
        C *d_clusters = nullptr;
        uint32_t *d_count = nullptr;
        const size_t pbytes = npx * sizeof(float);
        CUDA_CHECK(cudaMalloc(&d_mean, pbytes));
        CUDA_CHECK(cudaMalloc(&d_sum, pbytes));
        CUDA_CHECK(cudaMalloc(&d_sum2, pbytes));
        CUDA_CHECK(cudaMalloc(&d_off, pbytes));
        CUDA_CHECK(cudaMalloc(&d_frame, npx * sizeof(uint16_t)));
        CUDA_CHECK(cudaMalloc(&d_clusters, max_clusters * sizeof(C)));
        CUDA_CHECK(cudaMalloc(&d_count, sizeof(uint32_t)));

        cudaStream_t st = nullptr;
        CUDA_CHECK(cudaStreamCreate(&st));
        CUDA_CHECK(cudaMemcpyAsync(d_mean, img.mean.data(), pbytes,
                                   cudaMemcpyHostToDevice, st));
        CUDA_CHECK(cudaMemcpyAsync(d_sum, img.sum.data(), pbytes,
                                   cudaMemcpyHostToDevice, st));
        CUDA_CHECK(cudaMemcpyAsync(d_sum2, img.sum2.data(), pbytes,
                                   cudaMemcpyHostToDevice, st));
        CUDA_CHECK(cudaMemcpyAsync(d_off, offset.data(), pbytes,
                                   cudaMemcpyHostToDevice, st));

        const typename Algo::DeviceState state{
            d_frame, d_mean, d_sum, d_sum2, d_off, d_clusters, d_count};
        const cuda::LaunchParams params{nrows, ncols, nSigma, ped.n_samples(),
                                        max_clusters};

        std::vector<C> h_clusters(max_clusters);
        uint32_t h_count = 0;
        for (int f = 0; f < n_frames; ++f) {
            CUDA_CHECK(cudaMemsetAsync(d_count, 0, sizeof(uint32_t), st));
            CUDA_CHECK(cudaMemcpyAsync(d_frame, s.sig.data() + f * npx,
                                       npx * sizeof(uint16_t),
                                       cudaMemcpyHostToDevice, st));
            Algo::launch(params, state, st);
            CUDA_CHECK(cudaMemcpyAsync(&h_count, d_count, sizeof(uint32_t),
                                       cudaMemcpyDeviceToHost, st));
            CUDA_CHECK(cudaStreamSynchronize(st));
            const uint32_t n = std::min(h_count, max_clusters);
            CUDA_CHECK(cudaMemcpy(h_clusters.data(), d_clusters, n * sizeof(C),
                                  cudaMemcpyDeviceToHost));
            out.manual[f].assign(h_clusters.begin(), h_clusters.begin() + n);
        }

        CUDA_CHECK(cudaStreamDestroy(st));
        CUDA_CHECK(cudaFree(d_mean));
        CUDA_CHECK(cudaFree(d_sum));
        CUDA_CHECK(cudaFree(d_sum2));
        CUDA_CHECK(cudaFree(d_off));
        CUDA_CHECK(cudaFree(d_frame));
        CUDA_CHECK(cudaFree(d_clusters));
        CUDA_CHECK(cudaFree(d_count));
    }

    // ---- Route 2: the driver. One stream so the per-frame pedestal update
    // follows the same sequence as route 1. --------------------------------
    {
        ClusterFinderCUDA<Algo> cf(shape, nSigma, max_clusters, 1);
        for (int f = 0; f < n_ped; ++f)
            cf.push_pedestal_frame(NDView<uint16_t, 2>(
                const_cast<uint16_t *>(s.ped.data() + f * npx), shape));
        auto res = cf.find_clusters_batched(
            NDView<uint16_t, 3>(
                const_cast<uint16_t *>(s.sig.data()),
                {static_cast<ssize_t>(n_frames), shape[0], shape[1]}),
            0);
        for (int f = 0; f < n_frames; ++f)
            for (size_t i = 0; i < res[f].size(); ++i)
                out.driver[f].push_back(res[f][i]);
    }
    return out;
}

} // namespace

ManualVsDriver<Cluster<int32_t, 3, 3>>
run_fixed_window_3x3(int nrows, int ncols, int n_ped, int n_frames) {
    return run<3>(nrows, ncols, n_ped, n_frames);
}
ManualVsDriver<Cluster<int32_t, 9, 9>>
run_fixed_window_9x9(int nrows, int ncols, int n_ped, int n_frames) {
    return run<9>(nrows, ncols, n_ped, n_frames);
}

} // namespace aare::test_cuda
