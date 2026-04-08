// SPDX-License-Identifier: MPL-2.0
#pragma once
#include <cuda_runtime.h>
#include <cstdint>
#include <cmath>
#include <type_traits>
#include <stdexcept>
#include <cstdio>
#include "aare/Cluster.hpp"
#include "aare/utils/cuda_check.cuh"

namespace aare::device {

// Copied this struct from ClusterFinder.hpp
// but may be just include header?
template <typename ClusterType,
          typename = std::enable_if_t<is_cluster_v<ClusterType>>>
struct no_2x2_cluster {
    constexpr static bool value =
        ClusterType::cluster_size_x > 2 && ClusterType::cluster_size_y > 2;
};

// __________________
//
// CUDA kernel
// __________________

template <typename ClusterType = Cluster<int32_t, 3, 3>,
          typename FRAME_TYPE = uint16_t,
          typename PEDESTAL_TYPE = double,
          typename = std::enable_if_t<no_2x2_cluster<ClusterType>::value>>
__global__ void find_clusters_in_single_frame(const FRAME_TYPE*   __restrict__ d_frame,
                                              const PEDESTAL_TYPE* __restrict__ d_pd_mean,
                                              const PEDESTAL_TYPE* __restrict__ d_pd_sum2,
                                              const uint32_t       n_pd_samples,
                                              const PEDESTAL_TYPE  m_nSigma,
                                              const size_t         nrows,
                                              const size_t         ncols,
                                              const uint64_t       frame_number,
                                              ClusterType*         d_clusters,
                                              uint32_t*            d_cluster_count,
                                             const uint32_t       max_clusters)
{
    using CT = typename ClusterType::value_type;

    // Compile-time cluster geometry useful for unrolling loops
    constexpr uint8_t CSX = ClusterType::cluster_size_x;
    constexpr uint8_t CSY = ClusterType::cluster_size_y;
    constexpr int col_radius = CSX / 2;
    constexpr int row_radius = CSY / 2;

    // Squared threshold constants; avoids sqrt at runtime
    // c2^2 is for the 2x2 quadrant test
    // c3^2 is for the full-cluster total test
    constexpr int pow2_c2 = ((CSY + 1) / 2) * ((CSX + 1) / 2);
    constexpr int pow2_c3 = CSX * CSY;

    // Thread/pixel mapping
    auto col_global = static_cast<ssize_t>(threadIdx.x + blockDim.x * blockIdx.x);
    auto row_global = static_cast<ssize_t>(threadIdx.y + blockDim.y * blockIdx.y);
    auto global_tid = static_cast<ssize_t>(col_global + ncols * row_global);
    auto local_tid  = threadIdx.x + blockDim.x * threadIdx.y;

    // ====================
    // Shared memory layout
    // ====================
    // The tile is laid out contiguously in a 1D configuration:
    //    [0 ... tile_size-1] = pedestal-subtracted frame values
    //
    // Each tile has (blockDim.x + 2*col_radius) x (blockDim.y + 2*row_radius)
    // elements, with a halo of col_radius/row_radius pixels on each side.

    // CUDA prefers raw bytes + aligned cast
    // Compile error happens when using: `extern __shared__ T sh[];`
    extern __shared__ __align__(sizeof(PEDESTAL_TYPE)) unsigned char smem[];
    PEDESTAL_TYPE* shmem = reinterpret_cast<PEDESTAL_TYPE*>(smem);

    // Stride includes halo on both sides
    auto shmem_stride = static_cast<int>(blockDim.x) + 2 * col_radius;
    auto tile_size    = shmem_stride * (static_cast<int>(blockDim.y) + 2 * row_radius);

    // Offset so that thread (0,0) maps to shared-memory position
    // (row_radius, col_radius) i.e. past the top-left halo.
    auto shmem_tid = (static_cast<int>(threadIdx.y) + row_radius) * shmem_stride
                   + (static_cast<int>(threadIdx.x) + col_radius);

    // Cooperative zero-fill
    for (int idx = static_cast<int>(local_tid); idx < tile_size; idx += static_cast<int>(blockDim.x * blockDim.y)) {
        shmem[idx] = PEDESTAL_TYPE{0};
    }
    __syncthreads();

    // OOB flag
    bool valid_pixel = col_global < static_cast<ssize_t>(ncols) &&
                       row_global < static_cast<ssize_t>(nrows);

    // ======================================================
    // Load pedestal-subtracted frame data into shared memory
    // ======================================================

    // Helper: read (frame - pedestal_mean) from global memory, or 0 if OOB.
    // gr, gc are the global row/col of the pixel to load.
    // Returns the pedestal-subtracted value.
    auto load_pixel = [&] __device__ (ssize_t gr, ssize_t gc) -> PEDESTAL_TYPE {
        auto gid = gc + static_cast<ssize_t>(ncols) * gr;
        return static_cast<PEDESTAL_TYPE>(d_frame[gid]) - d_pd_mean[gid];
    };

    // A. Interior: every valid thread loads its own pixel
    if (valid_pixel) {
        shmem[shmem_tid] = load_pixel(row_global, col_global);
    }

    // B. Halo regions (Boundaries)
    // B.1  Top rows of the halo
    if (threadIdx.y == 0 && valid_pixel) {
        for (int i = 1; i <= row_radius; ++i) {
            if (row_global - i >= 0)
                shmem[shmem_tid - i * shmem_stride] = load_pixel(row_global - i, col_global);
        }
        // Top-left corner rectangle
        if (threadIdx.x == 0) {
            for (int i = 1; i <= row_radius; ++i)
                for (int j = 1; j <= col_radius; ++j)
                    if (row_global - i >= 0 && col_global - j >= 0)
                        shmem[shmem_tid - i * shmem_stride - j] = load_pixel(row_global - i, col_global - j);
        }
        // Top-right corner rectangle
        if (threadIdx.x == blockDim.x - 1) {
            for (int i = 1; i <= row_radius; ++i)
                for (int j = 1; j <= col_radius; ++j)
                    if (row_global - i >= 0 && col_global + j < static_cast<ssize_t>(ncols))
                        shmem[shmem_tid - i * shmem_stride + j] = load_pixel(row_global - i, col_global + j);
        }
    }

    // B.2  Left column of the halo
    if (threadIdx.x == 0 && valid_pixel) {
        for (int j = 1; j <= col_radius; ++j)
            if (col_global - j >= 0)
                shmem[shmem_tid - j] = load_pixel(row_global, col_global - j);
    }

    // B.3  Right column of the halo
    if (threadIdx.x == blockDim.x - 1 && valid_pixel) {
        for (int j = 1; j <= col_radius; ++j)
            if (col_global + j < static_cast<ssize_t>(ncols))
                shmem[shmem_tid + j] = load_pixel(row_global, col_global + j);
    }

    // B.4  Bottom rows of the halo
    if (threadIdx.y == blockDim.y - 1 && valid_pixel) {
        for (int i = 1; i <= row_radius; ++i) {
            if (row_global + i < static_cast<ssize_t>(nrows))
                shmem[shmem_tid + i * shmem_stride] = load_pixel(row_global + i, col_global);
        }
        // Bottom-left corner rectangle
        if (threadIdx.x == 0) {
            for (int i = 1; i <= row_radius; ++i)
                for (int j = 1; j <= col_radius; ++j)
                    if (row_global + i < static_cast<ssize_t>(nrows) && col_global - j >= 0)
                        shmem[shmem_tid + i * shmem_stride - j] = load_pixel(row_global + i, col_global - j);
        }
        // Bottom-right corner rectangle
        if (threadIdx.x == blockDim.x - 1) {
            for (int i = 1; i <= row_radius; ++i)
                for (int j = 1; j <= col_radius; ++j)
                    if (row_global + i < static_cast<ssize_t>(nrows) && col_global + j < static_cast<ssize_t>(ncols))
                        shmem[shmem_tid + i * shmem_stride + j] = load_pixel(row_global + i, col_global + j);
        }
    }

    __syncthreads();

    // =====================
    // Cluster-finding logic
    // =====================
    if (!valid_pixel) return;

    // Per-pixel RMS from global pedestal arrays
    // rms = sqrt( E[X^2] - E[X]^2 )
    PEDESTAL_TYPE mean_px  = d_pd_mean[global_tid];
    PEDESTAL_TYPE var_px   = d_pd_sum2[global_tid] / n_pd_samples - mean_px * mean_px;
    PEDESTAL_TYPE rms_sq  = max(var_px, PEDESTAL_TYPE{0}); // variance = rms^2
    PEDESTAL_TYPE rms_px  = sqrt(rms_sq);

    // Pedestal-subtracted value of the center pixel (already in shmem)
    PEDESTAL_TYPE val_pixel = shmem[shmem_tid];

    // Negative pedestal early exit 
    if (val_pixel < -m_nSigma * rms_px)
        return;  // NOTE: pedestal update for this pixel is skipped (same as sequential)

    // Stencil reduction: total, max, quadrant sums
    PEDESTAL_TYPE total = PEDESTAL_TYPE{0};
    PEDESTAL_TYPE max_val = -HUGE_VAL; // CUDA-compatible equivalent of `numeric_limits<double>::lowest()`
    PEDESTAL_TYPE tl = PEDESTAL_TYPE{0};   // top-left quadrant  (ir<=0, ic<=0)
    PEDESTAL_TYPE tr = PEDESTAL_TYPE{0};   // top-right quadrant (ir<=0, ic>=0)
    PEDESTAL_TYPE bl = PEDESTAL_TYPE{0};   // bottom-left        (ir>=0, ic<=0)
    PEDESTAL_TYPE br = PEDESTAL_TYPE{0};   // bottom-right       (ir>=0, ic>=0)

    CT clusterData[CSX * CSY];
    int idx = 0; // tracks the pixels in the cluster

    #pragma unroll
    for (int ir = -row_radius; ir <= row_radius; ++ir) {
        #pragma unroll
        for (int ic = -col_radius; ic <= col_radius; ++ic) {
            PEDESTAL_TYPE val = shmem[shmem_tid + ir * shmem_stride + ic];

            total += val;
            max_val = max(max_val, val);

            // Quadrant accumulation (pixels on the axes contribute to two quadrants)
            if (ir <= 0 && ic <= 0) tl += val;
            if (ir <= 0 && ic >= 0) tr += val;
            if (ir >= 0 && ic <= 0) bl += val;
            if (ir >= 0 && ic >= 0) br += val;

            // Store pedestal-subtracted value in register array for later cluster output
            if constexpr (std::is_integral_v<CT> && std::is_floating_point_v<PEDESTAL_TYPE>)
                clusterData[idx] = static_cast<CT>(lround(val));
            else
                clusterData[idx] = static_cast<CT>(val);

            idx++;
        }
    }

    // Three-way classification (mirrors ClusterFinder's logic)
    //
    //  1. Single-pixel significance:  max_val > nSigma * rms
    //     -> only the pixel that IS the max gets recorded (local-max suppression)
    //
    //  2. Quadrant significance:      max(tl,tr,bl,br) > c2 * nSigma * rms
    //     -> charge-sharing events where a 2x2 sub-region is significant (incomplete in ClusterFinder)
    //
    //  3. Total significance:         total > c3 * nSigma * rms
    //     -> distributed events where the full cluster sum is significant
    //
    //  NOTE: Tried to use squared comparisons, whenever possible,
    //  to avoid the slower `sqrt()` on c2 and c3:
    //     (x > c * nSigma * rms)  <-->  (x > 0 && x² > c² * nSigma² * rms²)

    PEDESTAL_TYPE nSig_sq_rms_sq = m_nSigma * m_nSigma * rms_sq;

    bool is_photon = false;

    // Test 1: single-pixel significance
    if (max_val > m_nSigma * rms_px) {
        // Local-max suppression: only the center-pixel thread records the cluster
        if (val_pixel < max_val)
            return;  // some other pixel in the neighborhood is brighter
        is_photon = true;
    }

    // Test 2: quadrant significance (only if test 1 didn't fire)
    if (!is_photon) {
        PEDESTAL_TYPE max_quad = max(max(tl, tr), max(bl, br));
        if (max_quad > 0 && max_quad * max_quad > pow2_c2 * nSig_sq_rms_sq) {
            is_photon = true;
        }
    }

    // Test 3: total significance (only if tests 1 & 2 didn't fire)
    if (!is_photon) {
        if (total > 0 && total * total > pow2_c3 * nSig_sq_rms_sq) {
            is_photon = true;
        }
    }

    // Pedestal update (if not a photon) 
    // TODO: Pedestal update is omitted here. In the sequential code,
    // non-photon pixels feed back into the running pedestal via push_fast().
    // On GPU this requires atomic updates to d_pd_sum / d_pd_sum2 which
    // possibly degrade performance...
    if (!is_photon)
        return;

    // Write cluster to global output buffer using atomic index 
    // for coordination across all blocks
    uint32_t write_idx = atomicAdd(d_cluster_count, 1u);

    // Guard against overflowing the pre-allocated cluster buffer
    if (write_idx >= max_clusters)
        return;

    ClusterType cluster{};
    cluster.x = static_cast<decltype(cluster.x)>(col_global);
    cluster.y = static_cast<decltype(cluster.y)>(row_global);
    
    // CT* cdata = cluster.data(); 
    
    // #pragma unroll
    // for (int i = 0; i < CSX * CSY; ++i) {
    //     cdata[i] = clusterData[i];
    // }
    memcpy(reinterpret_cast<CT*>(&cluster.data), clusterData, sizeof(CT) * CSX * CSY);    

    d_clusters[write_idx] = cluster;
}


// __________________
//
// Host-side launcher
// __________________
template <typename ClusterType = Cluster<int32_t, 3, 3>,
          typename FRAME_TYPE = uint16_t,
          typename PEDESTAL_TYPE = double>
struct ClusterFinderCUDA {

    static constexpr int BLOCK_X = 16;
    static constexpr int BLOCK_Y = 16;
    static constexpr int col_radius = ClusterType::cluster_size_x / 2;
    static constexpr int row_radius = ClusterType::cluster_size_y / 2;

    size_t nrows;
    size_t ncols;
    uint32_t max_clusters;

    // Device pointers
    FRAME_TYPE*    d_frame          = nullptr;
    PEDESTAL_TYPE* d_pd_mean        = nullptr;
    PEDESTAL_TYPE* d_pd_sum2        = nullptr;
    ClusterType*   d_clusters       = nullptr;
    uint32_t*      d_cluster_count  = nullptr;

    cudaStream_t   stream = 0;

    ClusterFinderCUDA(size_t nrows_, size_t ncols_, uint32_t max_clusters_ = 1000000)
        : nrows(nrows_), ncols(ncols_), max_clusters(max_clusters_)
    {
        size_t image_size = nrows * ncols;

        CUDA_CHECK(cudaStreamCreate(&stream));

        CUDA_CHECK(cudaMalloc(&d_frame,         image_size * sizeof(FRAME_TYPE)));
        CUDA_CHECK(cudaMalloc(&d_pd_mean,       image_size * sizeof(PEDESTAL_TYPE)));
        CUDA_CHECK(cudaMalloc(&d_pd_sum2,       image_size * sizeof(PEDESTAL_TYPE)));
        CUDA_CHECK(cudaMalloc(&d_clusters,      max_clusters * sizeof(ClusterType)));
        CUDA_CHECK(cudaMalloc(&d_cluster_count, sizeof(uint32_t)));
    }

    ~ClusterFinderCUDA() {
        if (d_frame)         cudaFree(d_frame);
        if (d_pd_mean)       cudaFree(d_pd_mean);
        if (d_pd_sum2)       cudaFree(d_pd_sum2);
        if (d_clusters)      cudaFree(d_clusters);
        if (d_cluster_count) cudaFree(d_cluster_count);
        if (stream)          cudaStreamDestroy(stream);
    }

    // Non-copyable, only movable
    ClusterFinderCUDA(const ClusterFinderCUDA&) = delete;
    ClusterFinderCUDA& operator=(const ClusterFinderCUDA&) = delete;
    ClusterFinderCUDA(ClusterFinderCUDA&&) = default;
    ClusterFinderCUDA& operator=(ClusterFinderCUDA&&) = default;

    /**
     * Upload pedestal statistics (mean and sum-of-squares) to device.
     * These are typically computed on the host by the Pedestal class.
     */
    void upload_pedestal(const PEDESTAL_TYPE* h_mean, const PEDESTAL_TYPE* h_sum2) {
        size_t bytes = nrows * ncols * sizeof(PEDESTAL_TYPE);
        CUDA_CHECK(cudaMemcpyAsync(d_pd_mean, h_mean, bytes, cudaMemcpyHostToDevice, stream));
        CUDA_CHECK(cudaMemcpyAsync(d_pd_sum2, h_sum2, bytes, cudaMemcpyHostToDevice, stream));
    }

    /**
     * Find clusters in a single frame.
     *
     * @param h_frame       Host pointer to the raw frame data (nrows x ncols)
     * @param frame_number  Frame sequence number
     * @param nSigma        Threshold in units of standard deviations
     * @param n_pd_samples  Number of samples used to compute the pedestal
     * @param h_clusters    Output: host buffer to receive found clusters
     * @param h_n_clusters  Output: number of clusters found
     */
    void find_clusters(const FRAME_TYPE* h_frame,
                       uint64_t frame_number,
                       PEDESTAL_TYPE nSigma,
                       uint32_t n_pd_samples,
                       ClusterType* h_clusters,
                       uint32_t& h_n_clusters)
    {
        size_t image_bytes = nrows * ncols * sizeof(FRAME_TYPE);

        // Reset cluster counter
        CUDA_CHECK(cudaMemsetAsync(d_cluster_count, 0, sizeof(uint32_t), stream));

        // Upload frame
        CUDA_CHECK(cudaMemcpyAsync(d_frame, h_frame, image_bytes, cudaMemcpyHostToDevice, stream));

        // Grid/Block dimensions
        dim3 block(BLOCK_X, BLOCK_Y);
        dim3 grid((static_cast<unsigned int>(ncols) + BLOCK_X - 1) / BLOCK_X, // threadIdx.x/.y/.z take unsigned integrales
                  (static_cast<unsigned int>(nrows) + BLOCK_Y - 1) / BLOCK_Y
        );

        // Shared memory: one tile of (BLOCK_X + 2*col_radius) x (BLOCK_Y + 2*row_radius) doubles
        size_t shmem_bytes = (BLOCK_X + 2 * col_radius) * (BLOCK_Y + 2 * row_radius) * sizeof(PEDESTAL_TYPE);

        // Kernel launch
        find_clusters_in_single_frame<ClusterType, FRAME_TYPE, PEDESTAL_TYPE>
            <<<grid, block, shmem_bytes, stream>>>(d_frame,
                                                   d_pd_mean,
                                                   d_pd_sum2,
                                                   n_pd_samples,
                                                   nSigma,
                                                   nrows,
                                                   ncols,
                                                   frame_number,
                                                   d_clusters,
                                                   d_cluster_count,
                                                   max_clusters
            );

        CUDA_CHECK(cudaGetLastError());

        // Read back cluster count
        CUDA_CHECK(cudaMemcpyAsync(&h_n_clusters, d_cluster_count, sizeof(uint32_t), cudaMemcpyDeviceToHost, stream));

        // Synchronize to ensure count is available before the CPU reads clusters
        CUDA_CHECK(cudaStreamSynchronize(stream));

        // Clamp to max in case of overflow
        if (h_n_clusters > max_clusters) h_n_clusters = max_clusters;

        // Read back clusters
        if (h_n_clusters > 0) {
            CUDA_CHECK(cudaMemcpyAsync(h_clusters, d_clusters, h_n_clusters * sizeof(ClusterType), cudaMemcpyDeviceToHost, stream));
            CUDA_CHECK(cudaStreamSynchronize(stream));
        }
    }
};

} // namespace aare::device
