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

/** enum to define the event types */
enum class eventType {
    PEDESTAL,   /** pedestal */
    NEIGHBOUR,  /** neighbour i.e. below threshold, but in the cluster of a
                   photon */
    PHOTON,     /** photon i.e. above threshold */
    PHOTON_MAX, /** maximum of a cluster satisfying the photon conditions */
    NEGATIVE_PEDESTAL, /** negative value, will not be accounted for as pedestal
                          in order to avoid drift of the pedestal towards
                          negative values */
    UNDEFINED_EVENT = -1 /** undefined */
};

template <typename FRAME_TYPE = uint16_t, typename PEDESTAL_TYPE = double,
          typename CT = int32_t>
class ClusterFinder {
    Shape<2> m_image_size;
    const int m_cluster_sizeX;
    const int m_cluster_sizeY;
    // const PEDESTAL_TYPE m_threshold;
    const PEDESTAL_TYPE m_nSigma;
    const PEDESTAL_TYPE c2;
    const PEDESTAL_TYPE c3;
    Pedestal<PEDESTAL_TYPE> m_pedestal;
    ClusterVector<CT> m_clusters;

  public:
    /**
     * @brief Construct a new ClusterFinder object
     * @param image_size size of the image
     * @param cluster_size size of the cluster (x, y)
     * @param nSigma number of sigma above the pedestal to consider a photon
     * @param capacity initial capacity of the cluster vector
     *
     */
    ClusterFinder(Shape<2> image_size, Shape<2> cluster_size,
                  PEDESTAL_TYPE nSigma = 5.0, size_t capacity = 1000000)
        : m_image_size(image_size), m_cluster_sizeX(cluster_size[0]),
          m_cluster_sizeY(cluster_size[1]),
          m_nSigma(nSigma),
          c2(sqrt((m_cluster_sizeY + 1) / 2 * (m_cluster_sizeX + 1) / 2)),
          c3(sqrt(m_cluster_sizeX * m_cluster_sizeY)),
          m_pedestal(image_size[0], image_size[1]),
          m_clusters(m_cluster_sizeX, m_cluster_sizeY, capacity) {};

    void push_pedestal_frame(NDView<FRAME_TYPE, 2> frame) {
        m_pedestal.push(frame);
    }

    NDArray<PEDESTAL_TYPE, 2> pedestal() { return m_pedestal.mean(); }
    NDArray<PEDESTAL_TYPE, 2> noise() { return m_pedestal.std(); }

    /**
     * @brief Move the clusters from the ClusterVector in the ClusterFinder to a
     * new ClusterVector and return it.
     * @param realloc_same_capacity if true the new ClusterVector will have the
     * same capacity as the old one
     *
     */
    ClusterVector<CT> steal_clusters(bool realloc_same_capacity = false) {
        ClusterVector<CT> tmp = std::move(m_clusters);
        if (realloc_same_capacity)
            m_clusters = ClusterVector<CT>(m_cluster_sizeX, m_cluster_sizeY,
                                           tmp.capacity());
        else
            m_clusters = ClusterVector<CT>(m_cluster_sizeX, m_cluster_sizeY);
        return tmp;
    }
    void find_clusters(NDView<FRAME_TYPE, 2> frame) {
        // // TODO! deal with even size clusters
        // // currently 3,3 -> +/- 1
        // //  4,4 -> +/- 2
        short dy = m_cluster_sizeY / 2;
        short dx = m_cluster_sizeX / 2;

        std::vector<CT> cluster_data(m_cluster_sizeX * m_cluster_sizeY);
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

                for (short ir = -dy; ir < dy + 1; ir++) {
                    for (short ic = -dx; ic < dx + 1; ic++) {
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
                    m_pedestal.push(iy, ix, frame(iy, ix));
                    continue; // It was a pedestal value nothing to store
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

    // // template <typename FRAME_TYPE, typename PEDESTAL_TYPE>
    // std::vector<DynamicCluster>
    // find_clusters_with_threshold(NDView<FRAME_TYPE, 2> frame,
    //                              Pedestal<PEDESTAL_TYPE> &pedestal) {
    //     assert(m_threshold > 0);
    //     std::vector<DynamicCluster> clusters;
    //     std::vector<std::vector<eventType>> eventMask;
    //     for (int i = 0; i < frame.shape(0); i++) {
    //         eventMask.push_back(std::vector<eventType>(frame.shape(1)));
    //     }
    //     double tthr, tthr1, tthr2;

    //     NDArray<FRAME_TYPE, 2> rest({frame.shape(0), frame.shape(1)});
    //     NDArray<int, 2> nph({frame.shape(0), frame.shape(1)});
    //     // convert to n photons
    //     // nph = (frame-pedestal.mean()+0.5*m_threshold)/m_threshold; // can
    //     be
    //     // optimized with expression templates?
    //     for (int iy = 0; iy < frame.shape(0); iy++) {
    //         for (int ix = 0; ix < frame.shape(1); ix++) {
    //             auto val = frame(iy, ix) - pedestal.mean(iy, ix);
    //             nph(iy, ix) = (val + 0.5 * m_threshold) / m_threshold;
    //             nph(iy, ix) = nph(iy, ix) < 0 ? 0 : nph(iy, ix);
    //             rest(iy, ix) = val - nph(iy, ix) * m_threshold;
    //         }
    //     }
    //     // iterate over frame pixels
    //     for (int iy = 0; iy < frame.shape(0); iy++) {
    //         for (int ix = 0; ix < frame.shape(1); ix++) {
    //             eventMask[iy][ix] = eventType::PEDESTAL;
    //             // initialize max and total
    //             FRAME_TYPE max = std::numeric_limits<FRAME_TYPE>::min();
    //             long double total = 0;
    //             if (rest(iy, ix) <= 0.25 * m_threshold) {
    //                 pedestal.push(iy, ix, frame(iy, ix));
    //                 continue;
    //             }
    //             eventMask[iy][ix] = eventType::NEIGHBOUR;
    //             // iterate over cluster pixels around the current pixel
    //             (ix,iy) for (short ir = -(m_cluster_sizeY / 2);
    //                  ir < (m_cluster_sizeY / 2) + 1; ir++) {
    //                 for (short ic = -(m_cluster_sizeX / 2);
    //                      ic < (m_cluster_sizeX / 2) + 1; ic++) {
    //                     if (ix + ic >= 0 && ix + ic < frame.shape(1) &&
    //                         iy + ir >= 0 && iy + ir < frame.shape(0)) {
    //                         auto val = frame(iy + ir, ix + ic) -
    //                                    pedestal.mean(iy + ir, ix + ic);
    //                         total += val;
    //                         if (val > max) {
    //                             max = val;
    //                         }
    //                     }
    //                 }
    //             }

    //             auto rms = pedestal.std(iy, ix);
    //             if (m_nSigma == 0) {
    //                 tthr = m_threshold;
    //                 tthr1 = m_threshold;
    //                 tthr2 = m_threshold;
    //             } else {
    //                 tthr = m_nSigma * rms;
    //                 tthr1 = m_nSigma * rms * c3;
    //                 tthr2 = m_nSigma * rms * c2;

    //                 if (m_threshold > 2 * tthr)
    //                     tthr = m_threshold - tthr;
    //                 if (m_threshold > 2 * tthr1)
    //                     tthr1 = tthr - tthr1;
    //                 if (m_threshold > 2 * tthr2)
    //                     tthr2 = tthr - tthr2;
    //             }
    //             if (total > tthr1 || max > tthr) {
    //                 eventMask[iy][ix] = eventType::PHOTON;
    //                 nph(iy, ix) += 1;
    //                 rest(iy, ix) -= m_threshold;
    //             } else {
    //                 pedestal.push(iy, ix, frame(iy, ix));
    //                 continue;
    //             }
    //             if (eventMask[iy][ix] == eventType::PHOTON &&
    //                 frame(iy, ix) - pedestal.mean(iy, ix) >= max) {
    //                 eventMask[iy][ix] = eventType::PHOTON_MAX;
    //                 DynamicCluster cluster(m_cluster_sizeX, m_cluster_sizeY,
    //                                        Dtype(typeid(FRAME_TYPE)));
    //                 cluster.x = ix;
    //                 cluster.y = iy;
    //                 short i = 0;
    //                 for (short ir = -(m_cluster_sizeY / 2);
    //                      ir < (m_cluster_sizeY / 2) + 1; ir++) {
    //                     for (short ic = -(m_cluster_sizeX / 2);
    //                          ic < (m_cluster_sizeX / 2) + 1; ic++) {
    //                         if (ix + ic >= 0 && ix + ic < frame.shape(1) &&
    //                             iy + ir >= 0 && iy + ir < frame.shape(0)) {
    //                             auto tmp = frame(iy + ir, ix + ic) -
    //                                        pedestal.mean(iy + ir, ix + ic);
    //                             cluster.set<FRAME_TYPE>(i, tmp);
    //                             i++;
    //                         }
    //                     }
    //                 }
    //                 clusters.push_back(cluster);
    //             }
    //         }
    //     }
    //     return clusters;
    // }
};

} // namespace aare