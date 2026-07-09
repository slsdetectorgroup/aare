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
    using IDX_1D_TYPE = uint32_t;
    using IDX_2D_TYPE = uint16_t;
    // Flag matrix marking pixels that are either active themselves or
    // neighbor an active pixel within a cluster window. It is built by
    // dilating around each active pixel as it is found in pass one, so
    // the cost is paid once per active pixel instead of once per pixel
    // in the frame, and pass three becomes a plain lookup.
    std::vector<uint8_t> m_near_candidate;

    // a list of coordinates of cluster candidates
    std::vector<IDX_1D_TYPE> m_photon_candidate_xy;

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

        // for triggering at 5/3 sigma (3x3), we should expect ~4% false
        // triggers for triggering at 5/5 sigma (5x5), we should expect ~15%
        // false triggers
        // TODO: the following constant should be controllable by the user
        const float expected_max_candidate_fraction = 0.2;
        const uint8_t dy = ClusterSizeY / 2;
        const uint8_t dx = ClusterSizeX / 2;
        const uint8_t has_center_pixel_x = ClusterSizeX % 2;
        const uint8_t has_center_pixel_y = ClusterSizeY % 2;

        const IDX_2D_TYPE ny = frame.shape(0);
        const IDX_2D_TYPE nx = frame.shape(1);

        m_clusters.set_frame_number(frame_number);

        // 1. Ensure capacity exists (only allocates ONCE at start)
        auto total_pixels = static_cast<size_t>(nx) * ny;
        if (m_near_candidate.size() != total_pixels) {
            // near candidate array needs to be allocated
            m_near_candidate.assign(total_pixels, 0);
            // on this occasion also reserve space for candidates
            m_photon_candidate_xy.reserve(
                static_cast<size_t>(ny * nx * expected_max_candidate_fraction));
        } else {
            // near_candidate array was already allocated
            // we can just reset the auxiliary data structures
            std::fill(m_near_candidate.begin(), m_near_candidate.end(), 0);
            m_photon_candidate_xy.clear();
        }

        // First pass - cheap single pixel test to build the candidate list
        // and dilate the flag matrix over the cluster window of every
        // active pixel found
        for (IDX_2D_TYPE iy = 0; iy < ny; iy++) {
            for (IDX_2D_TYPE ix = 0; ix < nx; ix++) {
                PEDESTAL_TYPE rms = m_pedestal.std(iy, ix);
                PEDESTAL_TYPE value = frame(iy, ix) - m_pedestal.mean(iy, ix);
                if (value > m_nSigma * rms / c3) {
                    m_photon_candidate_xy.push_back(to_1D_coord(ix, iy));
                }
            }
        }

        // Second pass - only for the (sparse) candidates, do the full 8
        // neighbor search and store clusters, same logic as find_clusters
        for (uint32_t k = 0; k < m_photon_candidate_xy.size(); k++) {
            auto [ix, iy] = to_2d_coord(m_photon_candidate_xy[k]);
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

            IDX_2D_TYPE y0 = std::max(0, iy - dy);
            IDX_2D_TYPE y1 = std::min(ny - 1, iy + dy - 1 + has_center_pixel_y);
            IDX_2D_TYPE x0 = std::max(0, ix - dx);
            IDX_2D_TYPE x1 = std::min(nx - 1, ix + dx - 1 + has_center_pixel_x);

            for (int ir = y0; ir <= y1; ir++) {
                std::fill(&m_near_candidate[ir * nx + x0],
                          &m_near_candidate[ir * nx + x1] + 1, 1);
            }

            ClusterType cluster{};
            cluster.x = ix;
            cluster.y = iy;

            uint32_t i = 0;
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
                if (m_near_candidate[iy * nx + ix])
                    continue;
                PEDESTAL_TYPE rms = m_pedestal.std(iy, ix);
                PEDESTAL_TYPE value = frame(iy, ix) - m_pedestal.mean(iy, ix);
                if (value < -m_nSigma * rms)
                    continue; // NEGATIVE_PEDESTAL, skip update
                m_pedestal.push_fast(iy, ix, frame(iy, ix));
            }
        }
    }

  private:
    IDX_1D_TYPE to_1D_coord(IDX_2D_TYPE x, IDX_2D_TYPE y) {
        constexpr uint8_t shift_dist = 8 * sizeof(IDX_1D_TYPE) / 2;
        constexpr IDX_1D_TYPE mask =
            (static_cast<IDX_1D_TYPE>(1) << shift_dist) - 1;
        return (static_cast<IDX_1D_TYPE>(x) << shift_dist) |
               (static_cast<IDX_1D_TYPE>(y) & mask);
    }

    std::pair<IDX_2D_TYPE, IDX_2D_TYPE> to_2d_coord(IDX_1D_TYPE xy) {
        constexpr uint8_t shift_dist = 8 * sizeof(IDX_1D_TYPE) / 2;
        constexpr IDX_1D_TYPE mask =
            (static_cast<IDX_1D_TYPE>(1) << shift_dist) - 1;
        return {static_cast<IDX_2D_TYPE>(xy >> shift_dist),
                static_cast<IDX_2D_TYPE>(xy & mask)};
    }
};

} // namespace aare