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

template <typename CT = int32_t, uint8_t ClusterSizeX = 3,
          uint8_t ClusterSizeY = 3, typename FRAME_TYPE = uint16_t,
          typename PEDESTAL_TYPE = double>
class ClusterFinder {
    Shape<2> m_image_size;
    const PEDESTAL_TYPE m_nSigma;
    const PEDESTAL_TYPE c2;
    const PEDESTAL_TYPE c3;
    Pedestal<PEDESTAL_TYPE> m_pedestal;
    ClusterVector<Cluster<CT, ClusterSizeX, ClusterSizeY>> m_clusters;

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
          m_pedestal(image_size[0], image_size[1]), m_clusters(capacity) {};

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
    ClusterVector<Cluster<CT, ClusterSizeX, ClusterSizeY>>
    steal_clusters(bool realloc_same_capacity = false) {
        ClusterVector<Cluster<CT, ClusterSizeX, ClusterSizeY>> tmp =
            std::move(m_clusters);
        if (realloc_same_capacity)
            m_clusters = ClusterVector<Cluster<CT, ClusterSizeX, ClusterSizeY>>(
                tmp.capacity());
        else
            m_clusters =
                ClusterVector<Cluster<CT, ClusterSizeX, ClusterSizeY>>{};
        return tmp;
    }
    void find_clusters(NDView<FRAME_TYPE, 2> frame, uint64_t frame_number = 0) {
        // // TODO! deal with even size clusters
        // // currently 3,3 -> +/- 1
        // //  4,4 -> +/- 2
        int dy = ClusterSizeY / 2;
        int dx = ClusterSizeX / 2;
        m_clusters.set_frame_number(frame_number);
        std::vector<CT> cluster_data(ClusterSizeX * ClusterSizeY);
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

                for (int ir = -dy; ir < dy + 1; ir++) {
                    for (int ic = -dx; ic < dx + 1; ic++) {
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
                    // Zero out the cluster data
                    std::fill(cluster_data.begin(), cluster_data.end(), 0);

                    // Fill the cluster data since we have a photon to store
                    // It's worth redoing the look since most of the time we
                    // don't have a photon
                    int i = 0;
                    for (int ir = -dy; ir < dy + 1; ir++) {
                        for (int ic = -dx; ic < dx + 1; ic++) {
                            if (ix + ic >= 0 && ix + ic < frame.shape(1) &&
                                iy + ir >= 0 && iy + ir < frame.shape(0)) {
                                CT tmp =
                                    static_cast<CT>(frame(iy + ir, ix + ic)) -
                                    m_pedestal.mean(iy + ir, ix + ic);
                                cluster_data[i] =
                                    tmp; // Watch for out of bounds access
                                i++;
                            }
                        }
                    }

                    // Add the cluster to the output ClusterVector
                    m_clusters.push_back(
                        ix, iy,
                        reinterpret_cast<std::byte *>(cluster_data.data()));
                }
            }
        }
    }
};

} // namespace aare