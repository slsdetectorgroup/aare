// SPDX-License-Identifier: MPL-2.0
#pragma once
#include <type_traits>
#include <utility>
#include <vector>

#include "aare/ClusterSink.hpp"
#include "aare/ClusterVector.hpp"
#include "aare/ProducerConsumerQueue.hpp"
#include "aare/defs.hpp"

namespace aare {

/**
 * @brief ClusterSink that accumulates all ClusterVectors in memory. Take them
 * with steal_clusters() after stop().
 */
template <typename ClusterType,
          typename = std::enable_if_t<is_cluster_v<ClusterType>>>
class ClusterCollector
    : public ClusterSink<ClusterCollector<ClusterType>, ClusterType> {
    using Base = ClusterSink<ClusterCollector<ClusterType>, ClusterType>;
    friend Base;

  public:
    using SourceQueue = typename Base::SourceQueue;

    /** @brief Construct an idle collector, start it with collect(). */
    ClusterCollector() = default;

    ~ClusterCollector() { this->stop(); }

  private:
    std::vector<ClusterVector<ClusterType>> m_clusters;

    void consume(ClusterVector<ClusterType> &&clusters) {
        m_clusters.push_back(std::move(clusters));
    }

    std::vector<ClusterVector<ClusterType>> steal_clusters_impl() {
        // guard by mutex if needed if this is called from multiple threads
        return std::move(m_clusters);
    }
};

} // namespace aare
