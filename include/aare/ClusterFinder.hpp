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
        for (int iy = 0; iy < frame.shape(0); iy++) {
            for (int ix = 0; ix < frame.shape(1); ix++) {

                PEDESTAL_TYPE max = std::numeric_limits<FRAME_TYPE>::min();
                PEDESTAL_TYPE total = 0;

                // What can we short circuit here?
                PEDESTAL_TYPE rms = m_pedestal.std(iy, ix);
                PEDESTAL_TYPE value = (frame(iy, ix) - m_pedestal.mean(iy, ix));

                if (value < -m_nSigma * rms)
                    continue; // NEGATIVE_PEDESTAL go to next pixel
                              // TODO! No pedestal update???

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
                    if (value < max)
                        continue; // Not max go to the next pixel
                                  // but also no pedestal update
                } else if (total > c3 * m_nSigma * rms) {
                    // pass
                } else {
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
    /// @brief Assumes that active pixels in the frame are rare
    ///        and utilizes this assumption to reduce the computation
    ///        cost of cluster finding from
    ///        O(cluster_tile_size*frame_size) to
    ///        O(cluster_tiles_size*active_pix_count + frame_size)
    void find_clusters_sparse(NDView<FRAME_TYPE, 2> frame,
                              uint64_t frame_number = 0) {

        const int expected_max_active_fraction = 1;
        int dy = ClusterSizeY / 2;
        int dx = ClusterSizeX / 2;
        int has_center_pixel_x = ClusterSizeX % 2;
        int has_center_pixel_y = ClusterSizeY % 2;

        const int ny = frame.shape(0);
        const int nx = frame.shape(1);

        m_clusters.set_frame_number(frame_number);

        // Flag matrix marking pixels that are either active themselves or
        // neighbor an active pixel within a cluster window. It is built by
        // dilating around each active pixel as it is found in pass one, so
        // the cost is paid once per active pixel instead of once per pixel
        // in the frame, and pass three becomes a plain lookup.
        std::vector<uint8_t> near_active(static_cast<size_t>(ny) * nx, 0);

        // Pre allocated candidate list. Worst case every pixel is active so
        // reserve for that up front and avoid any reallocation below
        // Potentially this could be a class member field only reserved once
        std::vector<int> active_x;
        std::vector<int> active_y;

        active_x.reserve(static_cast<size_t>(ny) * nx *
                         expected_max_active_fraction / 100);
        active_y.reserve(static_cast<size_t>(ny) * nx *
                         expected_max_active_fraction / 100);
        // First pass - cheap single pixel test to build the candidate list
        // and dilate the flag matrix over the cluster window of every
        // active pixel found
        for (int iy = 0; iy < ny; iy++) {
            for (int ix = 0; ix < nx; ix++) {
                PEDESTAL_TYPE rms = m_pedestal.std(iy, ix);
                PEDESTAL_TYPE value = frame(iy, ix) - m_pedestal.mean(iy, ix);
                if (value > m_nSigma * rms / c3) {
                    active_x.push_back(ix);
                    active_y.push_back(iy);

                    int y0 = std::max(0, iy - dy);
                    int y1 = std::min(ny - 1, iy + dy - 1 + has_center_pixel_y);
                    int x0 = std::max(0, ix - dx);
                    int x1 = std::min(nx - 1, ix + dx - 1 + has_center_pixel_x);

                    for (int ir = y0; ir <= y1; ir++) {
                        std::fill(&near_active[ir * nx + x0],
                                  &near_active[ir * nx + x1] + 1, 1);
                    }
                }
            }
        }

        // Second pass - only for the (sparse) candidates, do the full 8
        // neighbor search and store clusters, same logic as find_clusters
        for (size_t k = 0; k < active_x.size(); k++) {
            const int ix = active_x[k];
            const int iy = active_y[k];

            PEDESTAL_TYPE max = std::numeric_limits<FRAME_TYPE>::min();
            PEDESTAL_TYPE total = 0;
            PEDESTAL_TYPE rms = m_pedestal.std(iy, ix);
            PEDESTAL_TYPE value = frame(iy, ix) - m_pedestal.mean(iy, ix);

            for (int ir = -dy; ir < dy + has_center_pixel_y; ir++) {
                for (int ic = -dx; ic < dx + has_center_pixel_x; ic++) {
                    if (ix + ic >= 0 && ix + ic < nx && iy + ir >= 0 &&
                        iy + ir < ny) {
                        PEDESTAL_TYPE val = frame(iy + ir, ix + ic) -
                                            m_pedestal.mean(iy + ir, ix + ic);

                        total += val;
                        max = std::max(max, val);
                    }
                }
            }

            if (value != max ||
                (total <= c3 * m_nSigma * rms && value < m_nSigma * rms))
                continue; // not the local maximum, or not enough signal

            ClusterType cluster{};
            cluster.x = ix;
            cluster.y = iy;

            int i = 0;
            for (int ir = -dy; ir < dy + has_center_pixel_y; ir++) {
                for (int ic = -dx; ic < dx + has_center_pixel_x; ic++) {
                    if (ix + ic >= 0 && ix + ic < nx && iy + ir >= 0 &&
                        iy + ir < ny) {

                        if constexpr (std::is_integral_v<CT> &&
                                      std::is_floating_point_v<PEDESTAL_TYPE>) {
                            auto tmp =
                                std::lround(frame(iy + ir, ix + ic) -
                                            m_pedestal.mean(iy + ir, ix + ic));
                            cluster.data[i] = static_cast<CT>(tmp);
                        } else {
                            auto tmp = frame(iy + ir, ix + ic) -
                                       m_pedestal.mean(iy + ir, ix + ic);
                            cluster.data[i] = static_cast<CT>(tmp);
                        }
                    }
                    i++;
                }
            }

            m_clusters.push_back(cluster);
        }

        // Third pass - plain flag lookup, no neighbor search. Update the
        // pedestal for every pixel that is not active and does not
        // neighbor an active pixel.
        for (int iy = 0; iy < ny; iy++) {
            for (int ix = 0; ix < nx; ix++) {
                if (near_active[iy * nx + ix])
                    continue;
                PEDESTAL_TYPE rms = m_pedestal.std(iy, ix);
                PEDESTAL_TYPE value = frame(iy, ix) - m_pedestal.mean(iy, ix);
                if (value < -m_nSigma * rms)
                    continue; // NEGATIVE_PEDESTAL, skip update
                m_pedestal.push_fast(iy, ix, frame(iy, ix));
            }
        }
    }
};

} // namespace aare