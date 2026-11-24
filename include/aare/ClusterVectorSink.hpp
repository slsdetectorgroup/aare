// SPDX-License-Identifier: MPL-2.0
#pragma once
#include <atomic>
#include <mutex>
#include <thread>
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
class ClusterVectorSink
    : public ClusterSink<ClusterVectorSink<ClusterType>, ClusterType> {
    using Base = ClusterSink<ClusterVectorSink<ClusterType>, ClusterType>;
    friend Base; // Base calls consume() on the derived class

  public:
    using SourceQueue = typename Base::SourceQueue;

    /** @brief Construct an idle collector, start it with collect(). */
    ClusterVectorSink() = default;

    ~ClusterVectorSink() { this->stop(); }

    std::vector<ClusterVector<ClusterType>> steal_clusters() {
        // guard by mutex if needed if this is called from multiple threads
        {
            std::lock_guard<std::mutex> lock(m_clusters_mutex);
            return std::exchange(m_clusters, {});
        }
    }

  private:
    std::mutex m_clusters_mutex;
    std::vector<ClusterVector<ClusterType>> m_clusters;

    void consume(ClusterVector<ClusterType> &&clusters) {
        {
            std::lock_guard<std::mutex> lock(m_clusters_mutex);
            m_clusters.push_back(std::move(clusters));
        }
    }
};

} // namespace aare
