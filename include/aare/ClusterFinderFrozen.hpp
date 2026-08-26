// SPDX-License-Identifier: MPL-2.0
#pragma once
#include "aare/ClusterFile.hpp"
#include "aare/ClusterFinder.hpp" // no_2x2_cluster guard, reused here
#include "aare/ClusterVector.hpp"
#include "aare/Dtype.hpp"
#include "aare/NDArray.hpp"
#include "aare/NDView.hpp"
#include "aare/Pedestal.hpp"
#include "aare/defs.hpp"
#include <cstddef>
#include <utility>
#include <vector>

// --- ABLATION SWITCH (diagnostic, removable) -------------------------------
// Set to 0 to compile out Test3 (the total-significance test) while leaving
// every other decision untouched. Used to confirm, by an independent route from
// the branch-map trace, that Test3 is the channel through which pedestal update
// TIMING reaches the cluster set. Kept as an expression rather than #if so that
// `total` stays used and the two arms compile identically otherwise.
// --- AARE_BRANCH_TRACE (diagnostic, off by default) -------------------------
// 1 makes find_clusters() record which branch every pixel took, for the
// serial-vs-frozen study in annex A7 of the deck. Left at 0 the writes fold
// away at compile time, so the shipped path -- including the CPU baseline whose
// throughput the deck quotes -- is byte-for-byte what it was without this.
#ifndef AARE_BRANCH_TRACE
#define AARE_BRANCH_TRACE 0
#endif

#ifndef AARE_TEST3_ENABLED
#define AARE_TEST3_ENABLED 1
#endif

namespace aare {

/**
 * @brief Diagnostic twin of ClusterFinder that freezes the pedestal per frame.
 *
 * This class is BYTE-FOR-BYTE identical to ClusterFinder in every decision it
 * makes (negative skip, Test1 single-pixel significance, Test3 total
 * significance, the `value == max` store gate, edge handling, rounding). It
 * differs in exactly ONE respect: WHEN the pedestal is updated.
 *
 *   - ClusterFinder       : per-pixel. `push_fast` mutates the pedestal DURING
 *                           the raster scan, so a pixel's update can change the
 *                           decision for a later pixel in the same frame
 *                           (scan-order dependent).
 *
 *   - ClusterFinderFrozen : per-frame. Every decision in a frame reads a frozen
 *                           snapshot taken at frame start; all `push_fast`
 *                           updates are deferred to the end of the frame and so
 *                           only affect decisions from the NEXT frame on.
 *
 * That is precisely the CUDA kernel's model (frozen decision, batch update at
 * frame end). Running this variant against ClusterFinderCUDA isolates pedestal
 * update timing as the sole remaining variable: if the two agree, timing is the
 * proven cause of the CPU/CUDA mismatch.
 */
template <typename ClusterType = Cluster<int32_t, 3, 3>,
          typename FRAME_TYPE = uint16_t, typename PEDESTAL_TYPE = double,
          typename = std::enable_if_t<no_2x2_cluster<ClusterType>::value>>
class ClusterFinderFrozen {
    Shape<2> m_image_size;
    PEDESTAL_TYPE m_nSigma;
    const PEDESTAL_TYPE c2;
    const PEDESTAL_TYPE c3;
    Pedestal<PEDESTAL_TYPE> m_pedestal;
    ClusterVector<ClusterType> m_clusters;

    // --- AARE_BRANCH_TRACE (diagnostic, removable) ---------------------------
    // Per-pixel record of WHICH branch each pixel took this frame. Written to a
    // side buffer only; no decision reads it, so behaviour is unchanged.
    //   0 NEG          value < -nSigma*rms          (no update)
    //   1 SHADOW       Test1 pass, value < max      (no update)
    //   2 TEST1_STORE  Test1 pass, value == max     -> cluster
    //   3 TEST3_STORE  Test1 fail, Test3 pass, stored
    //   6 TEST3_SKIP   Test1 fail, Test3 pass, not stored (value < max)
    //   4 QUIET        both fail                    -> pedestal update
    //   5 UNTOUCHED    (frame not yet scanned here)
    std::vector<uint8_t> m_branch;

    static const uint8_t ClusterSizeX = ClusterType::cluster_size_x;
    static const uint8_t ClusterSizeY = ClusterType::cluster_size_y;
    using CT = typename ClusterType::value_type;

  public:
    ClusterFinderFrozen(Shape<2> image_size, PEDESTAL_TYPE nSigma = 5.0,
                        size_t capacity = 1000000)
        : m_image_size(image_size), m_nSigma(nSigma),
          c2(sqrt((ClusterSizeY + 1) / 2 * (ClusterSizeX + 1) / 2)),
          c3(sqrt(ClusterSizeX * ClusterSizeY)),
          m_pedestal(image_size[0], image_size[1]), m_clusters(capacity) {
        m_branch.assign(image_size[0] * image_size[1], 5);
        LOG(logDEBUG) << "ClusterFinderFrozen: "
                      << "image_size: " << image_size[0] << "x" << image_size[1]
                      << ", nSigma: " << nSigma << ", capacity: " << capacity;
    }

    void set_nSigma(PEDESTAL_TYPE nSigma) { m_nSigma = nSigma; }
    PEDESTAL_TYPE get_nSigma() const { return m_nSigma; }

    void push_pedestal_frame(NDView<FRAME_TYPE, 2> frame) {
        m_pedestal.push(frame);
    }

    NDArray<PEDESTAL_TYPE, 2> pedestal() { return m_pedestal.mean(); }
    NDArray<PEDESTAL_TYPE, 2> noise() { return m_pedestal.std(); }
    void clear_pedestal() { m_pedestal.clear(); }

    /// AARE_BRANCH_TRACE: last frame's per-pixel branch codes.
    NDArray<uint8_t, 2> branch_map() const {
        NDArray<uint8_t, 2> out({m_image_size[0], m_image_size[1]});
        std::copy(m_branch.begin(), m_branch.end(), out.begin());
        return out;
    }

    ClusterVector<ClusterType>
    steal_clusters(bool realloc_same_capacity = false) {
        ClusterVector<ClusterType> tmp = std::move(m_clusters);
        if (realloc_same_capacity)
            m_clusters = ClusterVector<ClusterType>(tmp.capacity());
        else
            m_clusters = ClusterVector<ClusterType>{};
        return tmp;
    }

    void find_clusters(NDView<FRAME_TYPE, 2> frame, uint64_t frame_number = 0) {
        int dy = ClusterSizeY / 2;
        int dx = ClusterSizeX / 2;
        int has_center_pixel_x = ClusterSizeX % 2;
        int has_center_pixel_y = ClusterSizeY % 2;

        m_clusters.set_frame_number(frame_number);
        if (AARE_BRANCH_TRACE)
            std::fill(m_branch.begin(), m_branch.end(), uint8_t{5});
        const int _bw = frame.shape(1);

        // FROZEN pedestal snapshot. Every decision this frame reads from these
        // copies, so an intra-frame push cannot influence a later pixel. This
        // mirrors the CUDA kernel, which decides the whole frame against the
        // pedestal as it stood at the start of the frame.
        NDArray<PEDESTAL_TYPE, 2> ped_mean = m_pedestal.mean();
        NDArray<PEDESTAL_TYPE, 2> ped_std = m_pedestal.std();

        // Pixels whose pedestal is updated at END of frame (deferred push),
        // exactly the set the serial finder would push_fast in-line.
        std::vector<std::pair<int, int>> deferred;

        for (int iy = 0; iy < frame.shape(0); iy++) {
            for (int ix = 0; ix < frame.shape(1); ix++) {

                PEDESTAL_TYPE max = std::numeric_limits<FRAME_TYPE>::min();
                PEDESTAL_TYPE total = 0;

                PEDESTAL_TYPE rms = ped_std(iy, ix);
                PEDESTAL_TYPE value = (frame(iy, ix) - ped_mean(iy, ix));

                if (value < -m_nSigma * rms) {
                    if (AARE_BRANCH_TRACE) m_branch[iy * _bw + ix] = 0;
                    continue; // NEGATIVE_PEDESTAL, no pedestal update
                }

                for (int ir = -dy; ir < dy + has_center_pixel_y; ir++) {
                    for (int ic = -dx; ic < dx + has_center_pixel_x; ic++) {
                        if (ix + ic >= 0 && ix + ic < frame.shape(1) &&
                            iy + ir >= 0 && iy + ir < frame.shape(0)) {
                            PEDESTAL_TYPE val = frame(iy + ir, ix + ic) -
                                                ped_mean(iy + ir, ix + ic);
                            total += val;
                            max = std::max(max, val);
                        }
                    }
                }

                if ((max > m_nSigma * rms)) {
                    if (value < max) {
                        if (AARE_BRANCH_TRACE) m_branch[iy * _bw + ix] = 1;
                        continue; // Not max, no pedestal update
                    }
                    if (AARE_BRANCH_TRACE) m_branch[iy * _bw + ix] = 2;
                } else if (AARE_TEST3_ENABLED && total > c3 * m_nSigma * rms) {
                    if (AARE_BRANCH_TRACE) m_branch[iy * _bw + ix] = (value == max) ? 3 : 6;
                } else {
                    if (AARE_BRANCH_TRACE) m_branch[iy * _bw + ix] = 4;
                    // Defer the pedestal update to end of frame (see below).
                    deferred.emplace_back(iy, ix);
                    continue;
                }

                // Store cluster
                if (value == max) {
                    ClusterType cluster{};
                    cluster.x = ix;
                    cluster.y = iy;

                    int i = 0;
                    for (int ir = -dy; ir < dy + has_center_pixel_y; ir++) {
                        for (int ic = -dx; ic < dx + has_center_pixel_x; ic++) {
                            if (ix + ic >= 0 && ix + ic < frame.shape(1) &&
                                iy + ir >= 0 && iy + ir < frame.shape(0)) {
                                if constexpr (std::is_integral_v<CT> &&
                                              std::is_floating_point_v<
                                                  PEDESTAL_TYPE>) {
                                    auto tmp =
                                        std::lround(frame(iy + ir, ix + ic) -
                                                    ped_mean(iy + ir, ix + ic));
                                    cluster.data[i] = static_cast<CT>(tmp);
                                } else {
                                    auto tmp = frame(iy + ir, ix + ic) -
                                               ped_mean(iy + ir, ix + ic);
                                    cluster.data[i] = static_cast<CT>(tmp);
                                }
                            }
                            i++;
                        }
                    }

                    m_clusters.push_back(cluster);
                }
            }
        }

        // End-of-frame pedestal update (deferred). Identical push_fast to the
        // serial finder, but every push now sees the SAME frozen baseline and
        // only affects decisions from the NEXT frame on -> matches CUDA. Each
        // pixel is visited at most once per frame, so push order is irrelevant.
        for (const auto &p : deferred)
            m_pedestal.push_fast(p.first, p.second, frame(p.first, p.second));
    }
};

} // namespace aare
