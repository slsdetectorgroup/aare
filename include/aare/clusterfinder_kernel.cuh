#pragma once
#include "aare/Cluster.hpp"
#include "aare/ClusterFinder.hpp"
#include <cuda_runtime.h>
#include <type_traits>

namespace aare::device {

// Implementing mixed precision for shared memory and stencil arithmetic
using COMPUTE_TYPE = float;

template <typename ClusterType = Cluster<int32_t, 3, 3>,
          typename FRAME_TYPE = uint16_t, typename PEDESTAL_TYPE = double,
          typename = std::enable_if_t<no_2x2_cluster<ClusterType>::value>>
__global__ void find_clusters_in_single_frame(
    const FRAME_TYPE *__restrict__ d_frame,
    PEDESTAL_TYPE *__restrict__ d_pd_mean, PEDESTAL_TYPE *__restrict__ d_pd_sum,
    PEDESTAL_TYPE *__restrict__ d_pd_sum2, const uint32_t n_pd_samples,
    const COMPUTE_TYPE m_nSigma, const size_t nrows, const size_t ncols,
    //   const uint64_t       frame_number,
    ClusterType *d_clusters, uint32_t *d_cluster_count,
    const uint32_t max_clusters) {
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
    auto col_global =
        static_cast<ssize_t>(threadIdx.x + blockDim.x * blockIdx.x);
    auto row_global =
        static_cast<ssize_t>(threadIdx.y + blockDim.y * blockIdx.y);
    auto global_tid = static_cast<ssize_t>(col_global + ncols * row_global);
    auto local_tid = threadIdx.x + blockDim.x * threadIdx.y;

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
    extern __shared__ __align__(sizeof(COMPUTE_TYPE)) unsigned char smem[];
    COMPUTE_TYPE *shmem = reinterpret_cast<COMPUTE_TYPE *>(smem);

    // Stride includes halo on both sides
    auto shmem_stride = static_cast<int>(blockDim.x) + 2 * col_radius;
    auto tile_size =
        shmem_stride * (static_cast<int>(blockDim.y) + 2 * row_radius);

    // Offset so that thread (0,0) maps to shared-memory position
    // (row_radius, col_radius) i.e. past the top-left halo.
    auto shmem_tid =
        (static_cast<int>(threadIdx.y) + row_radius) * shmem_stride +
        (static_cast<int>(threadIdx.x) + col_radius);

    // Cooperative zero-fill
    for (int idx = static_cast<int>(local_tid); idx < tile_size;
         idx += static_cast<int>(blockDim.x * blockDim.y)) {
        shmem[idx] = COMPUTE_TYPE{0};
    }
    __syncthreads();

    // OOB flag
    bool valid_pixel = col_global < static_cast<ssize_t>(ncols) &&
                       row_global < static_cast<ssize_t>(nrows);

    // ======================================================
    // Load pedestal-subtracted frame data into shared memory (MIXED PRECISION)
    // ======================================================

    // Helper: read (frame - pedestal_mean) from global memory, or 0 if OOB.
    // gr, gc are the global row/col of the pixel to load.
    // Returns the pedestal-subtracted value.
    auto load_pixel = [&] __device__(ssize_t gr, ssize_t gc) -> COMPUTE_TYPE {
        auto gid = gc + ncols * gr;
        return static_cast<COMPUTE_TYPE>(d_frame[gid]) -
               static_cast<COMPUTE_TYPE>(d_pd_mean[gid]);
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
                shmem[shmem_tid - i * shmem_stride] =
                    load_pixel(row_global - i, col_global);
        }
        // Top-left corner rectangle
        if (threadIdx.x == 0) {
            for (int i = 1; i <= row_radius; ++i)
                for (int j = 1; j <= col_radius; ++j)
                    if (row_global - i >= 0 && col_global - j >= 0)
                        shmem[shmem_tid - i * shmem_stride - j] =
                            load_pixel(row_global - i, col_global - j);
        }
        // Top-right corner rectangle
        if (threadIdx.x == blockDim.x - 1) {
            for (int i = 1; i <= row_radius; ++i)
                for (int j = 1; j <= col_radius; ++j)
                    if (row_global - i >= 0 &&
                        col_global + j < static_cast<ssize_t>(ncols))
                        shmem[shmem_tid - i * shmem_stride + j] =
                            load_pixel(row_global - i, col_global + j);
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
                shmem[shmem_tid + i * shmem_stride] =
                    load_pixel(row_global + i, col_global);
        }
        // Bottom-left corner rectangle
        if (threadIdx.x == 0) {
            for (int i = 1; i <= row_radius; ++i)
                for (int j = 1; j <= col_radius; ++j)
                    if (row_global + i < static_cast<ssize_t>(nrows) &&
                        col_global - j >= 0)
                        shmem[shmem_tid + i * shmem_stride - j] =
                            load_pixel(row_global + i, col_global - j);
        }
        // Bottom-right corner rectangle
        if (threadIdx.x == blockDim.x - 1) {
            for (int i = 1; i <= row_radius; ++i)
                for (int j = 1; j <= col_radius; ++j)
                    if (row_global + i < static_cast<ssize_t>(nrows) &&
                        col_global + j < static_cast<ssize_t>(ncols))
                        shmem[shmem_tid + i * shmem_stride + j] =
                            load_pixel(row_global + i, col_global + j);
        }
    }

    __syncthreads();

    // =====================
    // Cluster-finding logic
    // =====================
    if (!valid_pixel)
        return;

    // Per-pixel variance from global pedestal arrays
    // Variance = rms^2 = E[X^2] - E[X]^2
    // NOTE: Keep thresholds squared to avoid one sqrtf() per pixel.
    PEDESTAL_TYPE mean_px = d_pd_mean[global_tid];
    PEDESTAL_TYPE var_px =
        d_pd_sum2[global_tid] / n_pd_samples - mean_px * mean_px;
    PEDESTAL_TYPE rms_sq = max(var_px, PEDESTAL_TYPE{0}); // variance = rms^2
    PEDESTAL_TYPE nSig_sq_rms_sq = static_cast<PEDESTAL_TYPE>(m_nSigma) *
                                   static_cast<PEDESTAL_TYPE>(m_nSigma) *
                                   rms_sq;

    // Pedestal-subtracted value of the center pixel (already in shmem)
    COMPUTE_TYPE val_pixel = shmem[shmem_tid];

    // Negative pedestal early exit:
    //     val_pixel < -nSigma * rms
    // is equivalent to:
    //     val_pixel < 0 && val_pixel^2 > nSigma^2 * rms^2
    if (val_pixel < COMPUTE_TYPE{0} &&
        static_cast<PEDESTAL_TYPE>(val_pixel) *
                static_cast<PEDESTAL_TYPE>(val_pixel) >
            nSig_sq_rms_sq)
        return; // NOTE: pedestal update for this pixel is skipped (same as
                // sequential)

    // Stencil reduction: total, max, quadrant sums
    COMPUTE_TYPE total = 0.0f;
    COMPUTE_TYPE max_val = -HUGE_VALF;

    // // Quandrants
    // PEDESTAL_TYPE tl = PEDESTAL_TYPE{0};   // top-left quadrant  (ir<=0,
    // ic<=0) PEDESTAL_TYPE tr = PEDESTAL_TYPE{0};   // top-right quadrant
    // (ir<=0, ic>=0) PEDESTAL_TYPE bl = PEDESTAL_TYPE{0};   // bottom-left
    // (ir>=0, ic<=0) PEDESTAL_TYPE br = PEDESTAL_TYPE{0};   // bottom-right
    // (ir>=0, ic>=0)

#pragma unroll
    for (int ir = -row_radius; ir <= row_radius; ++ir) {
#pragma unroll
        for (int ic = -col_radius; ic <= col_radius; ++ic) {
            COMPUTE_TYPE val = shmem[shmem_tid + ir * shmem_stride + ic];

            total += val;
            max_val = fmaxf(max_val, val);

            // // Quadrant accumulation (pixels on the axes contribute to two
            // quadrants) if (ir <= 0 && ic <= 0) tl += val; if (ir <= 0 && ic
            // >= 0) tr += val; if (ir >= 0 && ic <= 0) bl += val; if (ir >= 0
            // && ic >= 0) br += val;
        }
    }

    // Three-way classification (mirrors ClusterFinder's logic)
    //
    //  1. Single-pixel significance:  max_val > nSigma * rms
    //     -> only the pixel that IS the max gets recorded (local-max
    //     suppression)
    //
    //  2. Quadrant significance:      max(tl,tr,bl,br) > c2 * nSigma * rms
    //     -> charge-sharing events where a 2x2 sub-region is significant
    //  NOTE: This test is absent in the serial ClusterFinder!
    //
    //  3. Total significance:         total > c3 * nSigma * rms
    //     -> distributed events where the full cluster sum is significant

    bool is_photon = false;

    // Test 1: single-pixel significance
    //     max_val > nSigma * rms
    // is equivalent to:
    //     max_val > 0 && max_val^2 > nSigma^2 * rms^2
    if (max_val > COMPUTE_TYPE{0} &&
        static_cast<PEDESTAL_TYPE>(max_val) *
                static_cast<PEDESTAL_TYPE>(max_val) >
            nSig_sq_rms_sq) {
        // Local-max suppression: only the center-pixel thread records the
        // cluster
        if (val_pixel < max_val)
            return; // some other pixel in the neighborhood is brighter
        is_photon = true;
    }

    /* // Test 2: quadrant significance (only if test 1 didn't fire)
    if (!is_photon) {
        PEDESTAL_TYPE max_quad = max(max(tl, tr), max(bl, br));
        if (max_quad > 0 && max_quad * max_quad > pow2_c2 * nSig_sq_rms_sq) {
            is_photon = true;
        }
    } */

    // Test 3: total significance (only if tests 1 & 2 didn't fire)
    if (!is_photon) {
        if (total > 0 && static_cast<PEDESTAL_TYPE>(total) *
                                 static_cast<PEDESTAL_TYPE>(total) >
                             pow2_c3 * nSig_sq_rms_sq) {
            is_photon = true;
        }
    }

    // Pedestal update (if not a photon)
    // In the sequential code, non-photon pixels feed back into the running
    // pedestal via push_fast(). In this kernel, the GPU updates all pixels in a
    // frame simultaneously. So the updated pedestal will only be used starting
    // from the next frame. -> This avoids a/serialization and b/global mem I/O.
    if (!is_photon && valid_pixel) {
        PEDESTAL_TYPE raw_val = static_cast<PEDESTAL_TYPE>(d_frame[global_tid]);
        PEDESTAL_TYPE sum = d_pd_sum[global_tid];
        PEDESTAL_TYPE sum2 = d_pd_sum2[global_tid];

        sum += raw_val - sum / n_pd_samples;
        sum2 += raw_val * raw_val - sum2 / n_pd_samples;

        d_pd_sum[global_tid] = sum;
        d_pd_sum2[global_tid] = sum2;
        d_pd_mean[global_tid] = sum / n_pd_samples;
        return;
    }

    /*
    if (!is_photon) return; // Debugging
    */

    // Delay building clusterData until we know this thread will write a photon.
    // This avoids CSX*CSY conversions/rounds for the overwhelmingly common
    // background pixels.
    CT clusterData[CSX * CSY];
    int idx = 0;

#pragma unroll
    for (int ir = -row_radius; ir <= row_radius; ++ir) {
#pragma unroll
        for (int ic = -col_radius; ic <= col_radius; ++ic) {
            COMPUTE_TYPE val = shmem[shmem_tid + ir * shmem_stride + ic];
            if constexpr (std::is_integral_v<CT>)
                clusterData[idx] = static_cast<CT>(lroundf(val));
            else
                clusterData[idx] = static_cast<CT>(val);
            idx++;
        }
    }

    // Write cluster to global output buffer using atomic index
    // for coordination across all blocks
    uint32_t write_idx = atomicAdd(d_cluster_count, 1u);

    // Guard against overflowing the pre-allocated cluster buffer
    if (write_idx >= max_clusters)
        return;

    ClusterType cluster{};
    cluster.x = static_cast<decltype(cluster.x)>(col_global);
    cluster.y = static_cast<decltype(cluster.y)>(row_global);

    memcpy(reinterpret_cast<CT *>(&cluster.data), clusterData,
           sizeof(CT) * CSX * CSY);

    d_clusters[write_idx] = cluster;
}

} // namespace aare::device