#pragma once
#include "aare/Cluster.hpp" // Cluster, no_2x2_cluster; nothing else from aare
#include <cmath>
#include <cstdint>
#include <cuda_runtime.h>
#include <type_traits>

namespace aare::device {

/// Stencil arithmetic precision. Set both aliases to double for an f64 build.
/// float is safe here because the device pedestal stores centered moments
/// against a host-computed baseline: f32 and f64 agree to 3e-7 in cluster
/// count, and f32 cuts the 9x9 kernel time by ~40%.
using COMPUTE_TYPE = float;
/// Device pedestal storage and running variance update. See COMPUTE_TYPE.
using DEVICE_PED_TYPE = float;

/// BLOCK_X/BLOCK_Y must equal the launch block dims; they are template
/// parameters so the shared-memory tile geometry is compile-time.
template <typename ClusterType = Cluster<int32_t, 3, 3>,
          typename FRAME_TYPE = uint16_t, int BLOCK_X = 16, int BLOCK_Y = 16,
          typename = std::enable_if_t<no_2x2_cluster<ClusterType>::value>>
__global__ void find_clusters_in_single_frame(
    const FRAME_TYPE *__restrict__ d_frame,
    DEVICE_PED_TYPE *__restrict__ d_pd_mean,
    DEVICE_PED_TYPE *__restrict__ d_pd_sum,
    DEVICE_PED_TYPE *__restrict__ d_pd_sum2,
    const DEVICE_PED_TYPE *__restrict__ d_pd_off, const uint32_t n_pd_samples,
    const COMPUTE_TYPE m_nSigma, const int32_t nrows, const int32_t ncols,
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
    // c2^2 is for the 2x2 quadrant test -- commented out along with Test 2
    //     itself (see below), whose disabled block is its only reader. Left in
    //     place rather than deleted so re-enabling Test 2 is one uncomment.
    // c3^2 is for the full-cluster total test
    // constexpr int pow2_c2 = ((CSY + 1) / 2) * ((CSX + 1) / 2);
    constexpr int pow2_c3 = CSX * CSY;

    // Shared-memory tile: the block's pixels plus a halo of
    // col_radius/row_radius on each side, laid out row-major.
    constexpr int TILE_W = BLOCK_X + 2 * col_radius;
    constexpr int TILE_H = BLOCK_Y + 2 * row_radius;
    constexpr int TILE_SIZE = TILE_W * TILE_H;
    constexpr int N_THREADS = BLOCK_X * BLOCK_Y;

    // Thread/pixel mapping. 32-bit signed: 64-bit integer math is emulated on
    // the GPU, and the bounds checks below rely on negative coordinates.
    const int col_global = static_cast<int>(threadIdx.x + BLOCK_X * blockIdx.x);
    const int row_global = static_cast<int>(threadIdx.y + BLOCK_Y * blockIdx.y);
    const int global_tid = col_global + ncols * row_global;
    const int local_tid = static_cast<int>(threadIdx.x + BLOCK_X * threadIdx.y);

    // CUDA prefers raw bytes + aligned cast
    // Compile error happens when using: `extern __shared__ T sh[];`
    extern __shared__ __align__(sizeof(COMPUTE_TYPE)) unsigned char smem[];
    COMPUTE_TYPE *shmem = reinterpret_cast<COMPUTE_TYPE *>(smem);

    // This thread's own pixel in the tile (past the top-left halo)
    const int shmem_tid =
        (static_cast<int>(threadIdx.y) + row_radius) * TILE_W +
        (static_cast<int>(threadIdx.x) + col_radius);

    // ==========================================================
    // Cooperative tile load: interior, halo and OOB zeros in one pass
    // ==========================================================
    // Every thread strides over the flat tile, so the halo costs the same
    // per thread as the interior instead of being serialised on edge lanes.
    // Cells outside the frame are written as 0, which is what the stencil
    // expects and removes the need for a separate zero-fill + barrier.
    const int tile_row0 = static_cast<int>(BLOCK_Y * blockIdx.y) - row_radius;
    const int tile_col0 = static_cast<int>(BLOCK_X * blockIdx.x) - col_radius;

#pragma unroll
    for (int idx = local_tid; idx < TILE_SIZE; idx += N_THREADS) {
        const int tr = idx / TILE_W; // constexpr divisor: mul + shift
        const int tc = idx - tr * TILE_W;
        const int gr = tile_row0 + tr;
        const int gc = tile_col0 + tc;
        COMPUTE_TYPE v = COMPUTE_TYPE{0};
        if (gr >= 0 && gr < nrows && gc >= 0 && gc < ncols) {
            const int gid = gc + ncols * gr;
            v = static_cast<COMPUTE_TYPE>(d_frame[gid]) - d_pd_mean[gid];
        }
        shmem[idx] = v;
    }
    __syncthreads();

    // =====================
    // Cluster-finding logic
    // =====================
    if (col_global >= ncols || row_global >= nrows)
        return;

    // Per-pixel variance from global pedestal arrays.
    // Variance = rms^2 = E[Y^2] - E[Y]^2, where Y = X - X0 is the pedestal
    // value centered on the frozen t=0 baseline X0 (= d_pd_off). The
    // accumulators hold the CENTERED moments (sum ~ n*E[Y], sum2 ~ n*E[Y^2]),
    // both O(rms), so this difference has no catastrophic cancellation even in
    // float — unlike the raw E[X^2] - E[X]^2 form, where X ~ 4600 makes both
    // terms ~2e7. NOTE: Keep thresholds squared to avoid one sqrtf() per pixel.
    const DEVICE_PED_TYPE mean_px = d_pd_mean[global_tid];
    const DEVICE_PED_TYPE resid = mean_px - d_pd_off[global_tid]; // E[Y] ~ O(1)
    const DEVICE_PED_TYPE var_px =
        d_pd_sum2[global_tid] / static_cast<DEVICE_PED_TYPE>(n_pd_samples) -
        resid * resid;
    const COMPUTE_TYPE rms_sq = static_cast<COMPUTE_TYPE>(
        var_px > DEVICE_PED_TYPE{0} ? var_px : DEVICE_PED_TYPE{0});
    const COMPUTE_TYPE nSig_sq_rms_sq = m_nSigma * m_nSigma * rms_sq;

    // Pedestal-subtracted value of the center pixel (already in shmem)
    const COMPUTE_TYPE val_pixel = shmem[shmem_tid];

    // Negative pedestal early exit:
    //     val_pixel < -nSigma * rms
    // is equivalent to:
    //     val_pixel < 0 && val_pixel^2 > nSigma^2 * rms^2
    if (val_pixel < 0.0f && val_pixel * val_pixel > nSig_sq_rms_sq)
        return; // NOTE: pedestal update for this pixel is skipped (same as
                // sequential)

    // Stencil reduction: total, max, quadrant sums
    COMPUTE_TYPE total = 0.0f;
    COMPUTE_TYPE max_val = -HUGE_VAL; // double inf; narrows to float if needed

    // // Quandrants
    // PEDESTAL_TYPE tl = PEDESTAL_TYPE{0};   // top-left quadrant  (ir<=0,
    // ic<=0) PEDESTAL_TYPE tr = PEDESTAL_TYPE{0};   // top-right quadrant
    // (ir<=0, ic>=0) PEDESTAL_TYPE bl = PEDESTAL_TYPE{0};   // bottom-left
    // (ir>=0, ic<=0) PEDESTAL_TYPE br = PEDESTAL_TYPE{0};   // bottom-right
    // (ir>=0, ic>=0)

    // Outer loop rolled on purpose: fully unrolling both loops lets the
    // scheduler hoist all CSX*CSY shared loads, which at 9x9 costs ~96
    // registers and caps occupancy at 2 blocks/SM. One unrolled row (9 loads
    // in flight) is enough to cover shared-memory latency.
#pragma unroll 1
    for (int ir = -row_radius; ir <= row_radius; ++ir) {
#pragma unroll
        for (int ic = -col_radius; ic <= col_radius; ++ic) {
            COMPUTE_TYPE val = shmem[shmem_tid + ir * TILE_W + ic];

            total += val;
            max_val = val > max_val ? val : max_val;

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
    if (max_val > 0.0f && max_val * max_val > nSig_sq_rms_sq) {
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
        if (total > 0.0f && total * total > static_cast<COMPUTE_TYPE>(pow2_c3) *
                                                nSig_sq_rms_sq) {
            // Local-max suppression, mirroring ClusterFinder's `value == max`
            // store gate: only the peak pixel of the window records the
            // cluster, so an extended charge-shared event yields one cluster,
            // not one per pixel. Return (not fall-through) because the serial
            // total branch pushes no pedestal for these non-max pixels.
            if (val_pixel < max_val)
                return;
            is_photon = true;
        }
    }

    // Pedestal update (if not a photon)
    // In the sequential code, non-photon pixels feed back into the running
    // pedestal via push_fast(). In this kernel, the GPU updates all pixels in a
    // frame simultaneously. So the updated pedestal will only be used starting
    // from the next frame. -> This avoids a/serialization and b/global mem I/O.
    if (!is_photon) {
        // Update the CENTERED moments of Y = X - X0 (X0 = frozen baseline).
        // Both sum (~n*E[Y]) and sum2 (~n*E[Y^2]) stay O(rms)-scale, so the
        // running EMA and its float rounding never touch the ~1e10 magnitudes
        // the raw accumulators would reach. Reconstruct the full mean as
        // X0 + sum/n for the pedestal subtraction on the next frame.
        const DEVICE_PED_TYPE X0 = d_pd_off[global_tid];
        const DEVICE_PED_TYPE y =
            static_cast<DEVICE_PED_TYPE>(d_frame[global_tid]) - X0;
        DEVICE_PED_TYPE sum = d_pd_sum[global_tid];
        DEVICE_PED_TYPE sum2 = d_pd_sum2[global_tid];
        const DEVICE_PED_TYPE n = static_cast<DEVICE_PED_TYPE>(n_pd_samples);

        sum += y - sum / n;
        sum2 += y * y - sum2 / n;

        d_pd_sum[global_tid] = sum;
        d_pd_sum2[global_tid] = sum2;
        d_pd_mean[global_tid] = X0 + sum / n;
        return;
    }

    /*
    if (!is_photon) return; // Debugging
    */

    // Claim an output slot (atomic index coordinates across all blocks)
    uint32_t write_idx = atomicAdd(d_cluster_count, 1u);

    // Guard against overflowing the pre-allocated cluster buffer
    if (write_idx >= max_clusters)
        return;

    // Convert straight from shmem into the global cluster. No local
    // CSX*CSY staging array: with the outer loop rolled it would be indexed
    // at runtime and land in local memory (328 B stack at 9x9).
    ClusterType *out = d_clusters + write_idx;
    out->x = static_cast<decltype(out->x)>(col_global);
    out->y = static_cast<decltype(out->y)>(row_global);
    CT *dst = reinterpret_cast<CT *>(&out->data);
    int idx = 0;

#pragma unroll 1
    for (int ir = -row_radius; ir <= row_radius; ++ir) {
#pragma unroll
        for (int ic = -col_radius; ic <= col_radius; ++ic) {
            COMPUTE_TYPE val = shmem[shmem_tid + ir * TILE_W + ic];
            if constexpr (std::is_integral_v<CT>)
                dst[idx] = static_cast<CT>(lround(val));
            else
                dst[idx] = static_cast<CT>(val);
            idx++;
        }
    }
}

} // namespace aare::device