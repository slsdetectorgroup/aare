// SPDX-License-Identifier: MPL-2.0
#pragma once
#include "aare/ClusterFinder.hpp"
#include "aare/ClusterVector.hpp"
#include "aare/ClusterVectorSink.hpp"

namespace aare {

/**
 * @brief A ClusterFinder that collects clusters in a ClusterVector in memory
 */
template <typename ClusterType,
          typename = std::enable_if_t<is_cluster_v<ClusterType>>>
class ClusterFinderVectorSink
    : public aare::ClusterFinder<aare::ClusterVectorSink<ClusterType>> {
  public:
    using Base = aare::ClusterFinder<aare::ClusterVectorSink<ClusterType>>;

    explicit ClusterFinderVectorSink(aare::Shape<2> image_size,
                                     double nSigma = 5.0,
                                     size_t capacity = 2000,
                                     size_t n_threads = 3,
                                     size_t queue_depth = 16,
                                     size_t min_pedestal_samples = 1000)
        : Base(image_size, nSigma, capacity, n_threads, queue_depth,
               min_pedestal_samples) {}

    ~ClusterFinderVectorSink() = default;
    ClusterFinderVectorSink(const ClusterFinderVectorSink &) = delete;
    ClusterFinderVectorSink &
    operator=(const ClusterFinderVectorSink &) = delete;
    ClusterFinderVectorSink(ClusterFinderVectorSink &&) = delete;
    ClusterFinderVectorSink &operator=(ClusterFinderVectorSink &&) = delete;

    /**
     * @brief Return all clusters currently accumulated in memory.
     */
    std::vector<ClusterVector<ClusterType>> steal_clusters() {
        return this->m_sink->steal_clusters();
    }

    /**
     * @brief Synchronizes finder and sink, wait for all frames to be processed
     * and written to the sink, then return all clusters currently accumulated
     * in memory.
     * @note If one used steal clusters beforehand only the remaining clusters
     * will be returned
     */
    std::vector<ClusterVector<ClusterType>> get_clusters() {
        this->sync();
        return this->m_sink->steal_clusters();
    }
};

} // end of namespace aare