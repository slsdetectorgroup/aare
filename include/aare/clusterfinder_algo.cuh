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
#include <type_traits>
#include <utility>
#include <vector>

#include "aare/RAII_device.hpp"

namespace aare::cuda {

/**
 * @brief The contract ClusterFinderCUDA<Algo> requires of its policies.
 *
 * C++17 has no concepts, so this list is the contract. It is documentation,
 * not machinery: a missing or misspelled member is reported clearly enough by
 * the compiler at the point of use. require_algo below asserts only the few
 * requirements that would otherwise compile and then misbehave at runtime.
 *
 * Ped (the pedestal model, e.g. CenteredRunningPedestal):
 *   DevicePedT                    element type of the device arrays
 *   Buffers                       owning, move-only, default-constructible
 *   View                          non-owning pointers the kernel takes
 *   Params                        per-launch pedestal scalars
 *   HostImage                     host image of Buffers, ready to upload
 *   Buffers   allocate(size_t n_pixels)
 *   View      make_view(const Buffers &)
 *   HostImage prepare_pedestal(Pedestal<HostPedT> &)   [template on HostPedT]
 *   void      upload_pedestal(const HostImage &, const Buffers &, cudaStream_t)
 *   void      download_mean(const Buffers &, size_t, std::vector<DevicePedT> &,
 *                           cudaStream_t)
 *   void      download_noise(const Buffers &, uint32_t, size_t,
 *                            std::vector<double> &, cudaStream_t)
 * The layout of View is between Ped and the kernel; the driver never reads it.
 *
 * Algo (the search algorithm, e.g. FixedWindow):
 *   cluster_type, frame_type, pedestal          pedestal must satisfy the above
 *   LaunchParams                                per-launch algorithm scalars
 *   Output                                      owning, move-only device output
 * block Output    Output::allocate(uint32_t max_clusters_per_frame) static
 * constexpr size_t Output::clusters_offset static size_t
 * Output::bytes_per_frame(uint32_t)   device and host stride uint8_t  *data()
 * const                    block base, for the D2H size_t    bytes() const
 *     uint32_t *count() const
 *     cluster_type *clusters() const
 *     uint32_t  Output::host_count(const void *, uint32_t)
 *     const cluster_type *Output::host_clusters(const void *)
 *   void launch(const LaunchParams &, const pedestal::Params &,
 *               const frame_type *, const pedestal::View &, const Output &,
 *               cudaStream_t)
 */
namespace contract {

/// Only the requirements that fail *silently* are asserted. A missing type or
/// a misspelled member already names itself at the point of use; a copyable
/// Output does not, it compiles and double-frees a device pointer.
template <class Algo> struct require_algo {
    using Ped = typename Algo::pedestal;
    using Output = typename Algo::Output;
    using Buffers = typename Ped::Buffers;

    static_assert(std::is_move_constructible<Output>::value &&
                      !std::is_copy_constructible<Output>::value,
                  "Algo::Output owns device memory: it must be move-only.");
    static_assert(std::is_move_constructible<Buffers>::value &&
                      !std::is_copy_constructible<Buffers>::value,
                  "Ped::Buffers owns device memory: it must be move-only.");
    static_assert(std::is_trivially_copyable<typename Ped::View>::value,
                  "Ped::View is passed to a kernel: it must be a trivially "
                  "copyable bundle of pointers, owning nothing.");
    static_assert(
        std::is_same<decltype(Output::clusters_offset), const size_t>::value &&
            std::is_same<decltype(Output::bytes_per_frame(0)), size_t>::value,
        "Algo::Output must expose the block layout as 'static constexpr size_t "
        "clusters_offset' and 'static size_t bytes_per_frame(uint32_t)': the "
        "driver sizes its pinned host slots from them, before any Output "
        "exists.");

    static constexpr bool value = true;
};

} // namespace contract

/**
 * @brief Per-pixel running pedestal, kept centered on a frozen baseline.
 *
 * Four device arrays per pixel: the full mean, and the first and second
 * moments of Y = X - off, where `off` is a per-pixel baseline frozen for the
 * lifetime of an uploaded image. Centering keeps the accumulators O(rms)
 * instead of O(1e10), which is what makes the single-precision running update
 * the kernel performs safe. See prepare_pedestal() for the host side.
 *
 * The state is bound to the sequence of frames launched against it: each frame
 * sees what its predecessors left. Two streams must not share one Buffers.
 *
 * Stateless policy. Every function is static, Buffers is the only owning type,
 * and the caller supplies it.
 */
struct CenteredRunningPedestal {
    using DevicePedT = device::DEVICE_PED_TYPE;

    struct Buffers {
        DeviceBuffer<DevicePedT> mean;
        DeviceBuffer<DevicePedT> sum;
        DeviceBuffer<DevicePedT> sum2;
        DeviceBuffer<DevicePedT> off;

        Buffers() = default;
        explicit Buffers(size_t n_pixels)
            : mean{n_pixels}, sum{n_pixels}, sum2{n_pixels}, off{n_pixels} {}

        Buffers(const Buffers &) = delete;
        Buffers &operator=(const Buffers &) = delete;

        Buffers(Buffers &&o) noexcept = default;
        Buffers &operator=(Buffers &&o) noexcept = default;
    };

    /// Non-owning. Every pointer is device memory owned by the caller.
    struct View {
        DevicePedT *d_pd_mean; ///< full mean = off + sum/n
        DevicePedT *d_pd_sum;  ///< ~ n * E[Y],   Y = X - off
        DevicePedT *d_pd_sum2; ///< ~ n * E[Y^2]
        const DevicePedT
            *d_pd_off; ///< frozen baseline, never written on device
    };

    struct Params {
        uint32_t n_samples;
    };

    /// Host image of the device pedestal arrays, ready to upload.
    struct HostImage {
        std::vector<DevicePedT> mean;
        std::vector<DevicePedT> sum;
        std::vector<DevicePedT> sum2;
        std::vector<DevicePedT> off;
    };

    static Buffers allocate(size_t n_pixels) { return Buffers{n_pixels}; }

    static View make_view(const Buffers &pedestal) {
        return View{pedestal.mean.get(), pedestal.sum.get(),
                    pedestal.sum2.get(), pedestal.off.get()};
    }

    /**
     * Turn a host pedestal into the centered device representation.
     *
     * The baseline X0 is the rounded host mean, so a re-prepare and re-upload
     * replace baseline and accumulators together and the device never rebases.
     *
     * Centering is done in double, where sum2 ~ n*E[X^2] is still exact, and
     * only the small centered results are narrowed to DevicePedT:
     *   sum(X - X0)   = sum(X)   - n*X0
     *   sum(X - X0)^2 = sum(X^2) - 2*X0*sum(X) + n*X0^2
     *
     * `mean` is deliberately NOT centered. It stays the full E[X] that the
     * kernel subtracts from a raw pixel every frame and writes back itself as
     * X0 + sum/n. At O(1e4) ADU float holds it comfortably; n*E[X^2] is the
     * term that would not survive.
     */
    template <typename HostPedT>
    static HostImage prepare_pedestal(Pedestal<HostPedT> &host) {
        NDArray<HostPedT, 2> h_mean = host.mean();
        NDArray<HostPedT, 2> h_sum = host.get_sum();
        NDArray<HostPedT, 2> h_sum2 = host.get_sum2();
        const size_t n_pixels = static_cast<size_t>(h_mean.size());
        const double n = static_cast<double>(host.n_samples());

        HostImage img;
        img.mean.resize(n_pixels);
        img.sum.resize(n_pixels);
        img.sum2.resize(n_pixels);
        img.off.resize(n_pixels);
        for (size_t i = 0; i < n_pixels; ++i) {
            const double X0 =
                static_cast<double>(std::llround(h_mean.data()[i]));
            const double s = static_cast<double>(h_sum.data()[i]);
            const double s2 = static_cast<double>(h_sum2.data()[i]);
            img.off[i] = static_cast<DevicePedT>(X0);
            img.mean[i] = static_cast<DevicePedT>(h_mean.data()[i]);
            img.sum[i] = static_cast<DevicePedT>(s - n * X0);
            img.sum2[i] =
                static_cast<DevicePedT>(s2 - 2.0 * X0 * s + n * X0 * X0);
        }
        return img;
    }

    /// Asynchronous H2D of a prepared image into one stream's buffers.
    static void upload_pedestal(const HostImage &h, const Buffers &d,
                                cudaStream_t stream) {
        const size_t bytes = h.mean.size() * sizeof(DevicePedT);
        CUDA_CHECK(cudaMemcpyAsync(d.mean.get(), h.mean.data(), bytes,
                                   cudaMemcpyHostToDevice, stream));
        CUDA_CHECK(cudaMemcpyAsync(d.sum.get(), h.sum.data(), bytes,
                                   cudaMemcpyHostToDevice, stream));
        CUDA_CHECK(cudaMemcpyAsync(d.sum2.get(), h.sum2.data(), bytes,
                                   cudaMemcpyHostToDevice, stream));
        CUDA_CHECK(cudaMemcpyAsync(d.off.get(), h.off.data(), bytes,
                                   cudaMemcpyHostToDevice, stream));
    }

    /// Synchronous D2H of the device mean (the pedestal the kernel decides
    /// with). Waits for `stream` first so an in-flight update has landed.
    static void download_mean(const Buffers &d, size_t n_pixels,
                              std::vector<DevicePedT> &out,
                              cudaStream_t stream) {
        CUDA_CHECK(cudaStreamSynchronize(stream));
        out.resize(n_pixels);
        CUDA_CHECK(cudaMemcpy(out.data(), d.mean.get(),
                              n_pixels * sizeof(DevicePedT),
                              cudaMemcpyDeviceToHost));
    }

    /// Synchronous D2H of the device rms, computed as the kernel does:
    /// sqrt(max(E[Y^2] - E[Y]^2, 0)) with E[Y] = mean - off.
    static void download_noise(const Buffers &d, uint32_t n_samples,
                               size_t n_pixels, std::vector<double> &out,
                               cudaStream_t stream) {
        CUDA_CHECK(cudaStreamSynchronize(stream));
        std::vector<DevicePedT> h_mean(n_pixels), h_sum2(n_pixels),
            h_off(n_pixels);
        CUDA_CHECK(cudaMemcpy(h_mean.data(), d.mean.get(),
                              n_pixels * sizeof(DevicePedT),
                              cudaMemcpyDeviceToHost));
        CUDA_CHECK(cudaMemcpy(h_sum2.data(), d.sum2.get(),
                              n_pixels * sizeof(DevicePedT),
                              cudaMemcpyDeviceToHost));
        CUDA_CHECK(cudaMemcpy(h_off.data(), d.off.get(),
                              n_pixels * sizeof(DevicePedT),
                              cudaMemcpyDeviceToHost));
        const double n = static_cast<double>(n_samples);
        out.resize(n_pixels);
        for (size_t i = 0; i < n_pixels; ++i) {
            const double resid =
                static_cast<double>(h_mean[i]) - static_cast<double>(h_off[i]);
            const double var =
                static_cast<double>(h_sum2[i]) / n - resid * resid;
            out[i] = std::sqrt(std::max(var, 0.0));
        }
    }
};

/**
 * @brief Fixed-size window cluster search on the GPU.
 *
 * One thread per pixel evaluates a cluster_size_x by cluster_size_y window
 * around it from a shared-memory tile, with local-maximum suppression so an
 * event yields one cluster. Non-photon pixels feed a per-pixel running
 * pedestal that is updated in place, frame by frame, on the device.
 *
 * This struct is the search half of the GPU path: the shape of the output one
 * frame writes, and the launch itself. The pedestal model is a separate
 * policy, `Ped`. It owns nothing and holds no state; every function is static
 * and every buffer is supplied by the caller.
 *
 * Two ways to use it:
 *  - through ClusterFinderCUDA<FixedWindow<...>>, which owns streams and
 *    buffers and pipelines batches over them;
 *  - directly, from code that manages its own device memory: allocate an
 *    Output, build a Ped::View over your pedestal arrays, and call launch()
 *    on your stream.
 */
template <typename ClusterType, typename FrameType = uint16_t,
          class Ped = CenteredRunningPedestal, int BLOCK_X = 16,
          int BLOCK_Y = 16>
struct FixedWindow {
    static_assert(no_2x2_cluster<ClusterType>::value,
                  "FixedWindow: both cluster dimensions must exceed 2");

    using cluster_type = ClusterType;
    using frame_type = FrameType;
    using pedestal = Ped;

    /// The output block one frame writes: a count word followed by the
    /// clusters, in one allocation. Owns the device copy and knows how to read
    /// a host copy of it. This is what a sparse finder replaces.
    struct Output {
        DeviceBuffer<uint8_t> d_block;

        /// Byte offset of the cluster array: the count word, padded up to the
        /// cluster alignment. A property of the layout, not of an allocation.
        static constexpr size_t clusters_offset =
            (sizeof(uint32_t) + alignof(ClusterType) - 1) &
            ~(alignof(ClusterType) - 1);

        /// Per-frame stride, on the device and in a pinned host slot. Static so
        /// the driver can size its host slots before any Output exists.
        static constexpr size_t bytes_per_frame(uint32_t max_clusters) {
            return clusters_offset +
                   static_cast<size_t>(max_clusters) * sizeof(ClusterType);
        }

        static Output allocate(uint32_t max_clusters_per_frame) {
            return Output{
                DeviceBuffer<uint8_t>(bytes_per_frame(max_clusters_per_frame))};
        }

        uint8_t *data() const { return d_block.get(); }
        size_t bytes() const { return d_block.bytes(); }

        uint32_t *count() const {
            return reinterpret_cast<uint32_t *>(d_block.get());
        }

        ClusterType *clusters() const {
            return reinterpret_cast<ClusterType *>(d_block.get() +
                                                   clusters_offset);
        }

        /// Readers for a host copy of one frame's block, used by the driver's
        /// collect(), collect_view() and materialize_slot().
        static uint32_t host_count(const void *h_block, uint32_t max_clusters) {
            // The device counter increments past the cap (only the write is
            // guarded), so this clamp is an out-of-bounds guard.
            return std::min(*reinterpret_cast<const uint32_t *>(h_block),
                            max_clusters);
        }

        static const ClusterType *host_clusters(const void *h_block) {
            return reinterpret_cast<const ClusterType *>(
                static_cast<const char *>(h_block) + clusters_offset);
        }
    };

    struct LaunchParams {
        int32_t nrows, ncols;
        float nSigma;
        uint32_t max_clusters;
    };

    /// Enqueue cluster finding for one frame on `stream`. Allocates nothing
    /// and synchronises nothing. The caller must have zeroed output.count().
    static void launch(const LaunchParams &lp, const typename Ped::Params &pp,
                       const FrameType *d_frame, const typename Ped::View &pv,
                       const Output &output, cudaStream_t stream) {
        constexpr int col_radius = ClusterType::cluster_size_x / 2;
        constexpr int row_radius = ClusterType::cluster_size_y / 2;
        constexpr size_t shmem_bytes = (BLOCK_X + 2 * col_radius) *
                                       (BLOCK_Y + 2 * row_radius) *
                                       sizeof(device::COMPUTE_TYPE);
        const dim3 block(BLOCK_X, BLOCK_Y);
        const dim3 grid(
            (static_cast<unsigned>(lp.ncols) + BLOCK_X - 1) / BLOCK_X,
            (static_cast<unsigned>(lp.nrows) + BLOCK_Y - 1) / BLOCK_Y);

        device::find_clusters_in_single_frame<ClusterType, FrameType, BLOCK_X,
                                              BLOCK_Y>
            <<<grid, block, shmem_bytes, stream>>>(
                d_frame, pv.d_pd_mean, pv.d_pd_sum, pv.d_pd_sum2, pv.d_pd_off,
                pp.n_samples, lp.nSigma, lp.nrows, lp.ncols, output.clusters(),
                output.count(), lp.max_clusters);
        CUDA_CHECK(cudaGetLastError());
    }
};

} // namespace aare::cuda
