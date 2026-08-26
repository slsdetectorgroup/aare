// SPDX-License-Identifier: MPL-2.0
#pragma once
#include "aare/ClusterFile.hpp"
#include "aare/ClusterVector.hpp"
#include "aare/Dtype.hpp"
#include "aare/NDArray.hpp"
#include "aare/NDView.hpp"
#include "aare/Pedestal.hpp"
#include "aare/defs.hpp"
#include <cstddef>

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

template <typename ClusterType,
          typename = std::enable_if_t<is_cluster_v<ClusterType>>>
struct no_2x2_cluster {
    constexpr static bool value =
        ClusterType::cluster_size_x > 2 && ClusterType::cluster_size_y > 2;
};

template <typename ClusterType = Cluster<int32_t, 3, 3>,
          typename FRAME_TYPE = uint16_t, typename PEDESTAL_TYPE = double,
          typename = std::enable_if_t<no_2x2_cluster<ClusterType>::value>>
class ClusterFinder {
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
    /**
     * @brief Construct a new ClusterFinder object
     * @param image_size size of the image
     * @param cluster_size size of the cluster (x, y)
     * @param nSigma number of sigma above the pedestal to consider a photon
     * @param capacity initial capacity of the cluster vector
     *
     */
    ClusterFinder(Shape<2> image_size, PEDESTAL_TYPE nSigma = 5.0,
                  size_t capacity = 1000000)
        : m_image_size(image_size), m_nSigma(nSigma),
          c2(sqrt((ClusterSizeY + 1) / 2 * (ClusterSizeX + 1) / 2)),
          c3(sqrt(ClusterSizeX * ClusterSizeY)),
          m_pedestal(image_size[0], image_size[1]), m_clusters(capacity) {
        m_branch.assign(image_size[0] * image_size[1], 5);
        LOG(logDEBUG) << "ClusterFinder: "
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

    /**
     * @brief Move the clusters from the ClusterVector in the ClusterFinder to a
     * new ClusterVector and return it.
     * @param realloc_same_capacity if true the new ClusterVector will have the
     * same capacity as the old one
     *
     */
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
        // // TODO! deal with even size clusters
        // // currently 3,3 -> +/- 1
        // //  4,4 -> +/- 2
        int dy = ClusterSizeY / 2;
        int dx = ClusterSizeX / 2;
        int has_center_pixel_x =
            ClusterSizeX %
            2; // for even sized clusters there is no proper cluster center and
               // even amount of pixels around the center
        int has_center_pixel_y = ClusterSizeY % 2;

        m_clusters.set_frame_number(frame_number);
        if (AARE_BRANCH_TRACE)
            std::fill(m_branch.begin(), m_branch.end(), uint8_t{5});
        const int _bw = frame.shape(1);
        for (int iy = 0; iy < frame.shape(0); iy++) {
            for (int ix = 0; ix < frame.shape(1); ix++) {

                PEDESTAL_TYPE max = std::numeric_limits<FRAME_TYPE>::min();
                PEDESTAL_TYPE total = 0;

                // What can we short circuit here?
                PEDESTAL_TYPE rms = m_pedestal.std(iy, ix);
                PEDESTAL_TYPE value = (frame(iy, ix) - m_pedestal.mean(iy, ix));

                if (value < -m_nSigma * rms) {
                    if (AARE_BRANCH_TRACE) m_branch[iy * _bw + ix] = 0;
                    continue; // NEGATIVE_PEDESTAL go to next pixel
                              // TODO! No pedestal update???
                }

                for (int ir = -dy; ir < dy + has_center_pixel_y; ir++) {
                    for (int ic = -dx; ic < dx + has_center_pixel_x; ic++) {
                        if (ix + ic >= 0 && ix + ic < frame.shape(1) &&
                            iy + ir >= 0 && iy + ir < frame.shape(0)) {
                            PEDESTAL_TYPE val =
                                frame(iy + ir, ix + ic) -
                                m_pedestal.mean(iy + ir, ix + ic);

                            total += val;
                            max = std::max(max, val);
                        }
                    }
                }

                if ((max > m_nSigma * rms)) {
                    if (value < max) {
                        if (AARE_BRANCH_TRACE) m_branch[iy * _bw + ix] = 1;
                        continue; // Not max go to the next pixel
                                  // but also no pedestal update
                    }
                    if (AARE_BRANCH_TRACE) m_branch[iy * _bw + ix] = 2;
                } else if (AARE_TEST3_ENABLED && total > c3 * m_nSigma * rms) {
                    if (AARE_BRANCH_TRACE) m_branch[iy * _bw + ix] = (value == max) ? 3 : 6;
                } else {
                    if (AARE_BRANCH_TRACE) m_branch[iy * _bw + ix] = 4;
                    // m_pedestal.push(iy, ix, frame(iy, ix));   // Safe option
                    m_pedestal.push_fast(
                        iy, ix,
                        frame(iy,
                              ix)); // Assume we have reached n_samples in the
                                    // pedestal, slight performance improvement
                    continue;       // It was a pedestal value nothing to store
                }

                // Store cluster
                if (value == max) {
                    ClusterType cluster{};
                    cluster.x = ix;
                    cluster.y = iy;

                    // Fill the cluster data since we have a photon to store
                    // It's worth redoing the look since most of the time we
                    // don't have a photon
                    int i = 0;
                    for (int ir = -dy; ir < dy + has_center_pixel_y; ir++) {
                        for (int ic = -dx; ic < dx + has_center_pixel_x; ic++) {
                            if (ix + ic >= 0 && ix + ic < frame.shape(1) &&
                                iy + ir >= 0 && iy + ir < frame.shape(0)) {

                                // If the cluster type is an integral type, and
                                // the pedestal is a floating point type then we
                                // need to round the value before storing it
                                if constexpr (std::is_integral_v<CT> &&
                                              std::is_floating_point_v<
                                                  PEDESTAL_TYPE>) {
                                    auto tmp = std::lround(
                                        frame(iy + ir, ix + ic) -
                                        m_pedestal.mean(iy + ir, ix + ic));
                                    cluster.data[i] = static_cast<CT>(tmp);
                                }
                                // On the other hand if both are floating point
                                // or both are integral then we can just static
                                // cast directly
                                else {
                                    auto tmp =
                                        frame(iy + ir, ix + ic) -
                                        m_pedestal.mean(iy + ir, ix + ic);
                                    cluster.data[i] = static_cast<CT>(tmp);
                                }
                            }
                            i++;
                        }
                    }

                    // Add the cluster to the output ClusterVector
                    m_clusters.push_back(cluster);
                }
            }
        }
    }
};

} // namespace aare