#pragma once
#include "aare/core/Dtype.hpp"
#include "aare/core/NDArray.hpp"
#include "aare/core/NDView.hpp"
#include "aare/processing/Pedestal.hpp"
#include "aare/core/Cluster.hpp"
#include <cstddef>

namespace aare {

/** enum to define the event types */
enum eventType {
    PEDESTAL,            /** pedestal */
    NEIGHBOUR,           /** neighbour i.e. below threshold, but in the cluster of a photon */
    PHOTON,              /** photon i.e. above threshold */
    PHOTON_MAX,          /** maximum of a cluster satisfying the photon conditions */
    NEGATIVE_PEDESTAL,   /** negative value, will not be accounted for as pedestal in order to
                             avoid drift of the pedestal towards negative values */
    UNDEFINED_EVENT = -1 /** undefined */
};

class ClusterFinder {
  public:
    ClusterFinder(int cluster_sizeX, int cluster_sizeY, double nSigma = 5.0, double threshold = 0.0)
        : m_cluster_sizeX(cluster_sizeX), m_cluster_sizeY(cluster_sizeY), m_threshold(threshold), m_nSigma(nSigma) {

        c2 = sqrt((cluster_sizeY + 1) / 2 * (cluster_sizeX + 1) / 2);
        c3 = sqrt(cluster_sizeX * cluster_sizeY);
    };

    template <typename FRAME_TYPE, typename PEDESTAL_TYPE>
    std::vector<ClusterData<PEDESTAL_TYPE,9>> find_clusters_without_threshold(NDView<FRAME_TYPE, 2> frame, Pedestal<PEDESTAL_TYPE> &pedestal,
                                                         bool late_update = false) {
        struct pedestal_update {
            int x;
            int y;
            PEDESTAL_TYPE value;
        };
        std::vector<pedestal_update> pedestal_updates;

        std::vector<deprecated::Cluster> clusters;
        std::vector<std::vector<eventType>> eventMask;
        for (int i = 0; i < frame.shape(0); i++) {
            eventMask.push_back(std::vector<eventType>(frame.shape(1)));
        }
        long double val;
        long double max;

        for (int iy = 0; iy < frame.shape(0); iy++) {
            for (int ix = 0; ix < frame.shape(1); ix++) {
                // initialize max and total
                max = std::numeric_limits<FRAME_TYPE>::min();
                long double total = 0;
                eventMask[iy][ix] = PEDESTAL;

                for (short ir = -(m_cluster_sizeY / 2); ir < (m_cluster_sizeY / 2) + 1; ir++) {
                    for (short ic = -(m_cluster_sizeX / 2); ic < (m_cluster_sizeX / 2) + 1; ic++) {
                        if (ix + ic >= 0 && ix + ic < frame.shape(1) && iy + ir >= 0 && iy + ir < frame.shape(0)) {
                            val = frame(iy + ir, ix + ic) - pedestal.mean(iy + ir, ix + ic);
                            total += val;
                            if (val > max) {
                                max = val;
                            }
                        }
                    }
                }
                auto rms = pedestal.standard_deviation(iy, ix);

                if (frame(iy, ix) - pedestal.mean(iy, ix) < -m_nSigma * rms) {
                    eventMask[iy][ix] = NEGATIVE_PEDESTAL;
                    continue;
                } else if (max > m_nSigma * rms) {
                    eventMask[iy][ix] = PHOTON;

                } else if (total > c3 * m_nSigma * rms) {
                    eventMask[iy][ix] = PHOTON;
                } else{
                    if (late_update) {
                        pedestal_updates.push_back({ix, iy, frame(iy, ix)});
                    } else {
                        pedestal.push(iy, ix, frame(iy, ix));
                    }
                    continue;
                }
                if (eventMask[iy][ix] == PHOTON && (frame(iy, ix) - pedestal.mean(iy, ix)) >= max) {
                    eventMask[iy][ix] = PHOTON_MAX;
                    deprecated::Cluster cluster(m_cluster_sizeX, m_cluster_sizeY, Dtype(typeid(PEDESTAL_TYPE)));
                    cluster.x = ix;
                    cluster.y = iy;
                    short i = 0;

                    for (short ir = -(m_cluster_sizeY / 2); ir < (m_cluster_sizeY / 2) + 1; ir++) {
                        for (short ic = -(m_cluster_sizeX / 2); ic < (m_cluster_sizeX / 2) + 1; ic++) {
                            if (ix + ic >= 0 && ix + ic < frame.shape(1) && iy + ir >= 0 && iy + ir < frame.shape(0)) {
                                PEDESTAL_TYPE tmp = static_cast<PEDESTAL_TYPE>(frame(iy + ir, ix + ic)) -
                                                    pedestal.mean(iy + ir, ix + ic);
                                cluster.set<PEDESTAL_TYPE>(i, tmp);
                                i++;
                            }
                        }
                    }
                    clusters.push_back(cluster);
                }
            }
        }
        if (late_update) {
            for (auto &update : pedestal_updates) {
                pedestal.push(update.y, update.x, update.value);
            }
        }
        return clusters;
    }

    template <typename FRAME_TYPE, typename PEDESTAL_TYPE>
    std::vector<deprecated::Cluster> find_clusters_with_threshold(NDView<FRAME_TYPE, 2> frame, Pedestal<PEDESTAL_TYPE> &pedestal) {
        assert(m_threshold > 0);
        std::vector<deprecated::Cluster> clusters;
        std::vector<std::vector<eventType>> eventMask;
        for (int i = 0; i < frame.shape(0); i++) {
            eventMask.push_back(std::vector<eventType>(frame.shape(1)));
        }
        double tthr, tthr1, tthr2;

        NDArray<FRAME_TYPE, 2> rest({frame.shape(0), frame.shape(1)});
        NDArray<int, 2> nph({frame.shape(0), frame.shape(1)});
        // convert to n photons
        // nph = (frame-pedestal.mean()+0.5*m_threshold)/m_threshold; // can be optimized with expression templates?
        for (int iy = 0; iy < frame.shape(0); iy++) {
            for (int ix = 0; ix < frame.shape(1); ix++) {
                auto val = frame(iy, ix) - pedestal.mean(iy, ix);
                nph(iy, ix) = (val + 0.5 * m_threshold) / m_threshold;
                nph(iy, ix) = nph(iy, ix) < 0 ? 0 : nph(iy, ix);
                rest(iy, ix) = val - nph(iy, ix) * m_threshold;
            }
        }
        // iterate over frame pixels
        for (int iy = 0; iy < frame.shape(0); iy++) {
            for (int ix = 0; ix < frame.shape(1); ix++) {
                eventMask[iy][ix] = PEDESTAL;
                // initialize max and total
                FRAME_TYPE max = std::numeric_limits<FRAME_TYPE>::min();
                long double total = 0;
                if (rest(iy, ix) <= 0.25 * m_threshold) {
                    pedestal.push(iy, ix, frame(iy, ix));
                    continue;
                }
                eventMask[iy][ix] = NEIGHBOUR;
                // iterate over cluster pixels around the current pixel (ix,iy)
                for (short ir = -(m_cluster_sizeY / 2); ir < (m_cluster_sizeY / 2) + 1; ir++) {
                    for (short ic = -(m_cluster_sizeX / 2); ic < (m_cluster_sizeX / 2) + 1; ic++) {
                        if (ix + ic >= 0 && ix + ic < frame.shape(1) && iy + ir >= 0 && iy + ir < frame.shape(0)) {
                            auto val = frame(iy + ir, ix + ic) - pedestal.mean(iy + ir, ix + ic);
                            total += val;
                            if (val > max) {
                                max = val;
                            }
                        }
                    }
                }

                auto rms = pedestal.standard_deviation(iy, ix);
                if (m_nSigma == 0) {
                    tthr = m_threshold;
                    tthr1 = m_threshold;
                    tthr2 = m_threshold;
                } else {
                    tthr = m_nSigma * rms;
                    tthr1 = m_nSigma * rms * c3;
                    tthr2 = m_nSigma * rms * c2;

                    if (m_threshold > 2 * tthr)
                        tthr = m_threshold - tthr;
                    if (m_threshold > 2 * tthr1)
                        tthr1 = tthr - tthr1;
                    if (m_threshold > 2 * tthr2)
                        tthr2 = tthr - tthr2;
                }
                if (total > tthr1 || max > tthr) {
                    eventMask[iy][ix] = PHOTON;
                    nph(iy, ix) += 1;
                    rest(iy, ix) -= m_threshold;
                } else {
                    pedestal.push(iy, ix, frame(iy, ix));
                    continue;
                }
                if (eventMask[iy][ix] == PHOTON && frame(iy, ix) - pedestal.mean(iy, ix) >= max) {
                    eventMask[iy][ix] = PHOTON_MAX;
                    deprecated::Cluster cluster(m_cluster_sizeX, m_cluster_sizeY, Dtype(typeid(FRAME_TYPE)));
                    cluster.x = ix;
                    cluster.y = iy;
                    short i = 0;
                    for (short ir = -(m_cluster_sizeY / 2); ir < (m_cluster_sizeY / 2) + 1; ir++) {
                        for (short ic = -(m_cluster_sizeX / 2); ic < (m_cluster_sizeX / 2) + 1; ic++) {
                            if (ix + ic >= 0 && ix + ic < frame.shape(1) && iy + ir >= 0 && iy + ir < frame.shape(0)) {
                                auto tmp = frame(iy + ir, ix + ic) - pedestal.mean(iy + ir, ix + ic);
                                cluster.set<FRAME_TYPE>(i, tmp);
                                i++;
                            }
                        }
                    }
                    clusters.push_back(cluster);
                }
            }
        }
        return clusters;
    }

  protected:
    int m_cluster_sizeX;
    int m_cluster_sizeY;
    double m_threshold;
    double m_nSigma;
    double c2;
    double c3;
};

} // namespace aare