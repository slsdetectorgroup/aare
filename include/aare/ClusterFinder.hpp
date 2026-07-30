// SPDX-License-Identifier: MPL-2.0
#pragma once
#include "aare/ClusterFile.hpp"
#include "aare/ClusterVector.hpp"
#include "aare/Dtype.hpp"
#include "aare/NDArray.hpp"
#include "aare/NDView.hpp"
#include "aare/Pedestal.hpp"
#include "aare/FastPedestal.hpp"
#include "aare/defs.hpp"
#include <cstddef>

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
    FastPedestal<PEDESTAL_TYPE> m_pedestal;
    ClusterVector<ClusterType> m_clusters;

    static const uint8_t ClusterSizeX = ClusterType::cluster_size_x;
    static const uint8_t ClusterSizeY = ClusterType::cluster_size_y;
    using CT = typename ClusterType::value_type;

    NDArray<PEDESTAL_TYPE, 2> m_threshold;
    NDArray<PEDESTAL_TYPE, 2> m_pd_corrected_frame;

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
          m_pedestal(image_size[0], image_size[1]), m_clusters(capacity),
          m_pd_corrected_frame({image_size[0], image_size[1]}, 0) {
        LOG(logDEBUG) << "ClusterFinder: "
                      << "image_size: " << image_size[0] << "x" << image_size[1]
                      << ", nSigma: " << nSigma << ", capacity: " << capacity;
    }

    void set_nSigma(PEDESTAL_TYPE nSigma) { m_nSigma = nSigma; }

    PEDESTAL_TYPE get_nSigma() const { return m_nSigma; }

    void push_pedestal_frame(NDView<FRAME_TYPE, 2> frame) {
        if (!m_pedestal.ready()) {
            m_pedestal.push_init(frame);
        } else {
            m_pedestal.push(frame);
        }
    }

    NDArray<PEDESTAL_TYPE, 2> pedestal() { return m_pedestal.mean(); }
    NDArray<PEDESTAL_TYPE, 2> noise() { return m_pedestal.std(); }
    void clear_pedestal() { m_pedestal.clear(); }

    /**
     * @brief Refresh the cached std of the underlying pedestal. Call before
     * reading the pedestal's cached std.
     */
    void update_std() { m_pedestal.update_std(); }

    void update_threshold() { m_threshold = m_pedestal.std() * m_nSigma; }

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
        constexpr int dy = ClusterSizeY / 2;
        constexpr int dx = ClusterSizeX / 2;
        constexpr int has_center_pixel_x =
            ClusterSizeX %
            2; // for even sized clusters there is no proper cluster center and
               // even amount of pixels around the center
        constexpr int has_center_pixel_y = ClusterSizeY % 2;

        m_clusters.set_frame_number(frame_number);

        m_pd_corrected_frame = frame - m_pedestal.view();
        for (int iy = 0; iy < frame.shape(0); iy++) {
            for (int ix = 0; ix < frame.shape(1); ix++) {

                PEDESTAL_TYPE max = std::numeric_limits<FRAME_TYPE>::min();
                PEDESTAL_TYPE total = 0;

                // What can we short circuit here?
                // PEDESTAL_TYPE rms = m_pedestal.cached_std(iy, ix);
                PEDESTAL_TYPE threshold = m_threshold(iy, ix);
                PEDESTAL_TYPE value = m_pd_corrected_frame(iy, ix);

                if (value < -threshold)
                    continue; // NEGATIVE_PEDESTAL go to next pixel
                              // TODO! No pedestal update???

                for (int ir = -dy; ir < dy + has_center_pixel_y; ir++) {
                    for (int ic = -dx; ic < dx + has_center_pixel_x; ic++) {
                        if (ix + ic >= 0 && ix + ic < frame.shape(1) &&
                            iy + ir >= 0 && iy + ir < frame.shape(0)) {
                            PEDESTAL_TYPE val =
                                m_pd_corrected_frame(iy + ir, ix + ic);

                            total += val;
                            max = std::max(max, val);
                        }
                    }
                }

                if ((max > threshold)) {
                    if (value < max)
                        continue; // Not max go to the next pixel
                                  // but also no pedestal update
                } else if (total > c3 * threshold) {
                    // pass
                } else {
                    // m_pedestal.push(iy, ix, frame(iy, ix));   // Safe option
                    m_pedestal.push(
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
                                        m_pd_corrected_frame(iy + ir, ix + ic));
                                    cluster.data[i] = static_cast<CT>(tmp);
                                }
                                // On the other hand if both are floating point
                                // or both are integral then we can just static
                                // cast directly
                                else {
                                    auto tmp =
                                        m_pd_corrected_frame(iy + ir, ix + ic);
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