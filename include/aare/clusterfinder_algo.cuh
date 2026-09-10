// SPDX-License-Identifier: MPL-2.0
#pragma once
#include "aare/Cluster.hpp"
#include "aare/NDArray.hpp"
#include "aare/Pedestal.hpp"
#include "aare/clusterfinder_kernel.cuh"
#include "aare/utils/cuda_check.cuh"
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cuda_runtime.h>
#include <utility>
#include <vector>

namespace aare::cuda {

/// Per-launch scalars every GPU cluster-finding algorithm takes. Kept
/// algorithm-agnostic so the driver (ClusterFinderCUDA) can fill it without
/// knowing which algorithm it is driving.
struct LaunchParams {
    int32_t nrows;
    int32_t ncols;
    float nSigma;
    uint32_t n_pd_samples; ///< EMA window of the running pedestal
    uint32_t max_clusters; ///< capacity of d_clusters; the counter may exceed
                           ///< it, only the write is guarded
};

/**
 * @brief Fixed-size window cluster search on the GPU.
 *
 * One thread per pixel evaluates a cluster_size_x by cluster_size_y window
 * around it from a shared-memory tile, with local-maximum suppression so an
 * event yields one cluster. Non-photon pixels feed a per-pixel running
 * pedestal that is updated in place, frame by frame, on the device.
 *
 * This struct is the algorithm-specific half of the GPU path: what device
 * state one launch needs, how to build that state from a host pedestal, how
 * to read it back, and the launch itself. It owns nothing and holds no state;
 * every function is static and every pointer is supplied by the caller.
 *
 * Two ways to use it:
 *  - through ClusterFinderCUDA<FixedWindow<...>>, which owns streams and
 *    buffers and pipelines batches over them;
 *  - directly, from code that manages its own device memory: fill a
 *    DeviceState from your pointers and call launch() on your stream.
 *
 * Pedestal representation. The device keeps four arrays per pixel: the full
 * mean, and CENTERED first and second moments of Y = X - off, where `off` is
 * a per-pixel baseline frozen at the first upload. Centering keeps the
 * accumulators O(rms) instead of O(1e10), which is what makes the
 * single-precision running update safe. prepare_pedestal() does the centering
 * on the host in double.
 *
 * The pedestal is bound to the sequence of frames launched against it: each
 * frame sees the state its predecessors left. Two streams must not share one
 * DeviceState.
 */
template <typename ClusterType, typename FrameType = uint16_t, int BLOCK_X = 16,
          int BLOCK_Y = 16>
struct FixedWindow {
    static_assert(no_2x2_cluster<ClusterType>::value,
                  "FixedWindow: both cluster dimensions must exceed 2");

    using cluster_type = ClusterType;
    using frame_type = FrameType;
    using ped_type = device::DEVICE_PED_TYPE;

    /// Non-owning. Every pointer is device memory owned by the caller.
    struct DeviceState {
        const FrameType *d_frame;
        ped_type *d_pd_mean;      ///< full mean = off + sum/n
        ped_type *d_pd_sum;       ///< ~ n * E[Y],   Y = X - off
        ped_type *d_pd_sum2;      ///< ~ n * E[Y^2]
        const ped_type *d_pd_off; ///< frozen baseline, never written on device
        ClusterType *d_clusters;
        uint32_t *d_count; ///< zero on entry; written by atomicAdd
    };

    /// Owning. The pedestal arrays one stream needs, allocated in the
    /// constructor and freed in the destructor. Move-only.
    struct PedestalBuffers {
        ped_type *mean = nullptr;
        ped_type *sum = nullptr;
        ped_type *sum2 = nullptr;
        ped_type *off = nullptr;

        explicit PedestalBuffers(size_t n_pixels) {
            const size_t bytes = n_pixels * sizeof(ped_type);
            CUDA_CHECK(cudaMalloc(&mean, bytes));
            CUDA_CHECK(cudaMalloc(&sum, bytes));
            CUDA_CHECK(cudaMalloc(&sum2, bytes));
            CUDA_CHECK(cudaMalloc(&off, bytes));
        }
        ~PedestalBuffers() {
            // Bare cudaFree: a failure at teardown is not actionable, and
            // cudaFree(nullptr) is a no-op for a moved-from object.
            cudaFree(mean);
            cudaFree(sum);
            cudaFree(sum2);
            cudaFree(off);
        }
        PedestalBuffers(PedestalBuffers &&o) noexcept { swap(o); }
        PedestalBuffers &operator=(PedestalBuffers &&o) noexcept {
            swap(o);
            return *this;
        }
        PedestalBuffers(const PedestalBuffers &) = delete;
        PedestalBuffers &operator=(const PedestalBuffers &) = delete;

        void swap(PedestalBuffers &o) noexcept {
            std::swap(mean, o.mean);
            std::swap(sum, o.sum);
            std::swap(sum2, o.sum2);
            std::swap(off, o.off);
        }
    };

    /// Assemble a DeviceState from owned buffers. Used by StreamContext;
    /// external callers build DeviceState directly from their own pointers.
    static DeviceState make_state(const FrameType *d_frame,
                                  const PedestalBuffers &p,
                                  ClusterType *d_clusters, uint32_t *d_count) {
        return DeviceState{d_frame, p.mean,     p.sum,  p.sum2,
                           p.off,   d_clusters, d_count};
    }

    /// Enqueue cluster finding for one frame on `stream`. Allocates nothing
    /// and synchronises nothing. `s.d_count` must already be zero.
    static void launch(const LaunchParams &p, const DeviceState &s,
                       cudaStream_t stream) {
        constexpr int col_radius = ClusterType::cluster_size_x / 2;
        constexpr int row_radius = ClusterType::cluster_size_y / 2;
        constexpr size_t shmem_bytes = (BLOCK_X + 2 * col_radius) *
                                       (BLOCK_Y + 2 * row_radius) *
                                       sizeof(device::COMPUTE_TYPE);
        const dim3 block(BLOCK_X, BLOCK_Y);
        const dim3 grid(
            (static_cast<unsigned>(p.ncols) + BLOCK_X - 1) / BLOCK_X,
            (static_cast<unsigned>(p.nrows) + BLOCK_Y - 1) / BLOCK_Y);

        device::find_clusters_in_single_frame<ClusterType, FrameType, BLOCK_X,
                                              BLOCK_Y>
            <<<grid, block, shmem_bytes, stream>>>(
                s.d_frame, s.d_pd_mean, s.d_pd_sum, s.d_pd_sum2, s.d_pd_off,
                p.n_pd_samples, p.nSigma, p.nrows, p.ncols, s.d_clusters,
                s.d_count, p.max_clusters);
        CUDA_CHECK(cudaGetLastError());
    }

    /// Host image of the device pedestal arrays, ready to upload.
    struct HostImage {
        std::vector<ped_type> mean;
        std::vector<ped_type> sum;
        std::vector<ped_type> sum2;
    };

    /**
     * Turn a host pedestal into the centered device representation.
     *
     * `offset` is the frozen per-pixel baseline. It is captured from the
     * host mean the first time this is called (when empty or mis-sized) and
     * left untouched afterwards, so the device accumulators never need
     * rebasing. Clear it to force a fresh capture.
     *
     * Centering is done in double, where sum2 ~ n*E[X^2] is still exact, and
     * only the small centered results are narrowed to ped_type:
     *   sum(X - X0)   = sum(X)   - n*X0
     *   sum(X - X0)^2 = sum(X^2) - 2*X0*sum(X) + n*X0^2
     */
    template <typename HostPedT>
    static HostImage prepare_pedestal(Pedestal<HostPedT> &host,
                                      std::vector<ped_type> &offset) {
        NDArray<HostPedT, 2> h_mean = host.mean();
        NDArray<HostPedT, 2> h_sum = host.get_sum();
        NDArray<HostPedT, 2> h_sum2 = host.get_sum2();
        const size_t n_pixels = static_cast<size_t>(h_mean.size());
        const double n = static_cast<double>(host.n_samples());

        if (offset.size() != n_pixels) {
            offset.resize(n_pixels);
            for (size_t i = 0; i < n_pixels; ++i)
                offset[i] =
                    static_cast<ped_type>(std::llround(h_mean.data()[i]));
        }

        HostImage img;
        img.mean.resize(n_pixels);
        img.sum.resize(n_pixels);
        img.sum2.resize(n_pixels);
        for (size_t i = 0; i < n_pixels; ++i) {
            const double X0 = static_cast<double>(offset[i]);
            const double s = static_cast<double>(h_sum.data()[i]);
            const double s2 = static_cast<double>(h_sum2.data()[i]);
            img.mean[i] = static_cast<ped_type>(h_mean.data()[i]);
            img.sum[i] = static_cast<ped_type>(s - n * X0);
            img.sum2[i] =
                static_cast<ped_type>(s2 - 2.0 * X0 * s + n * X0 * X0);
        }
        return img;
    }

    /// Asynchronous H2D of a prepared image into one stream's buffers.
    static void upload_pedestal(const HostImage &h,
                                const std::vector<ped_type> &offset,
                                const PedestalBuffers &d, cudaStream_t stream) {
        const size_t bytes = h.mean.size() * sizeof(ped_type);
        CUDA_CHECK(cudaMemcpyAsync(d.mean, h.mean.data(), bytes,
                                   cudaMemcpyHostToDevice, stream));
        CUDA_CHECK(cudaMemcpyAsync(d.sum, h.sum.data(), bytes,
                                   cudaMemcpyHostToDevice, stream));
        CUDA_CHECK(cudaMemcpyAsync(d.sum2, h.sum2.data(), bytes,
                                   cudaMemcpyHostToDevice, stream));
        CUDA_CHECK(cudaMemcpyAsync(d.off, offset.data(), bytes,
                                   cudaMemcpyHostToDevice, stream));
    }

    /// Synchronous D2H of the device mean (the pedestal the kernel decides
    /// with). Waits for `stream` first so an in-flight update has landed.
    static void download_mean(const PedestalBuffers &d, size_t n_pixels,
                              std::vector<ped_type> &out, cudaStream_t stream) {
        CUDA_CHECK(cudaStreamSynchronize(stream));
        out.resize(n_pixels);
        CUDA_CHECK(cudaMemcpy(out.data(), d.mean, n_pixels * sizeof(ped_type),
                              cudaMemcpyDeviceToHost));
    }

    /// Synchronous D2H of the device rms, computed as the kernel does:
    /// sqrt(max(E[Y^2] - E[Y]^2, 0)) with E[Y] = mean - off.
    static void download_noise(const PedestalBuffers &d,
                               const std::vector<ped_type> &offset,
                               uint32_t n_samples, size_t n_pixels,
                               std::vector<double> &out, cudaStream_t stream) {
        CUDA_CHECK(cudaStreamSynchronize(stream));
        std::vector<ped_type> h_mean(n_pixels), h_sum2(n_pixels);
        CUDA_CHECK(cudaMemcpy(h_mean.data(), d.mean,
                              n_pixels * sizeof(ped_type),
                              cudaMemcpyDeviceToHost));
        CUDA_CHECK(cudaMemcpy(h_sum2.data(), d.sum2,
                              n_pixels * sizeof(ped_type),
                              cudaMemcpyDeviceToHost));
        const double n = static_cast<double>(n_samples);
        out.resize(n_pixels);
        for (size_t i = 0; i < n_pixels; ++i) {
            const double resid =
                static_cast<double>(h_mean[i]) - static_cast<double>(offset[i]);
            const double var =
                static_cast<double>(h_sum2[i]) / n - resid * resid;
            out[i] = std::sqrt(std::max(var, 0.0));
        }
    }
};

} // namespace aare::cuda
