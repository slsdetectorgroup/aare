// SPDX-License-Identifier: MPL-2.0
#pragma once
#include <atomic>
#include <mutex>
#include <thread>
#include <type_traits>
#include <utility>
#include <vector>

#include "aare/Backoff.hpp"
#include "aare/ClusterVector.hpp"
#include "aare/ProducerConsumerQueue.hpp"
#include "aare/cf/ClusterFinderMT.hpp"
#include "aare/defs.hpp"

namespace aare {

/**
 * @brief Takes the clusters out of a sink queue, such as the one of a
 * ClusterFinderMT, on its own thread and keeps them in memory.
 *
 * The thread is started by the constructor and stopped by stop() or the
 * destructor, and can be started again with start(). The clusters collected
 * so far can be taken with steal_clusters() at any time, also while the
 * collector is running. start() and stop() must not be called from several
 * threads at once.
 *
 * The queue must outlive the collector.
 */
template <typename ClusterType,
          typename = std::enable_if_t<is_cluster_v<ClusterType>>>
class ClusterCollector {
    using SourceQueue = ProducerConsumerQueue<ClusterVector<ClusterType>>;

    SourceQueue *m_source;
    std::atomic<bool> m_stop_requested{false};
    std::thread m_thread;
    std::mutex m_clusters_mutex; // guards m_clusters
    std::vector<ClusterVector<ClusterType>> m_clusters;

    void process() {
        fmt::print("ClusterCollector started\n");
        Backoff backoff;
        while (true) {
            if (ClusterVector<ClusterType> *clusters = m_source->frontPtr();
                clusters != nullptr) {
                backoff.reset();
                {
                    std::lock_guard<std::mutex> lock(m_clusters_mutex);
                    m_clusters.push_back(std::move(*clusters));
                }
                // Removed from the queue only once it can be stolen, so an
                // empty queue means that nothing is on its way
                m_source->popFront();
                continue;
            }

            if (m_stop_requested.load(std::memory_order_acquire))
                break;

            backoff.pause();
        }
        fmt::print("ClusterCollector stopped\n");
    }

  public:
    explicit ClusterCollector(SourceQueue *source) : m_source(source) {
        // only one process does that so why isnt it automatically written to
        // m_cluster in collect - instead of writing first to m_sink?
        start();
    }

    template <typename Finder,
              std::enable_if_t<
                  std::is_same_v<decltype(std::declval<Finder &>().sink()),
                                 SourceQueue *>,
                  int> = 0>
    explicit ClusterCollector(Finder *source)
        : ClusterCollector(source->sink()) {}

    /// Stops the collector thread, see stop()
    ~ClusterCollector() { stop(); }

    // The thread holds a pointer to this object
    ClusterCollector(const ClusterCollector &) = delete;
    ClusterCollector &operator=(const ClusterCollector &) = delete;

    /**
     * @brief Start the collector thread. Does nothing if it is running.
     *
     * Clusters collected before stop() that have not been stolen are kept.
     */
    void start() {
        if (m_thread.joinable()) {
            return;
        }
        m_stop_requested.store(false, std::memory_order_release);
        m_thread = std::thread(&ClusterCollector::process, this);
    }

    /**
     * @brief Stop the collector thread once the queue is empty. Does nothing
     * if it is stopped.
     */
    void stop() {
        if (!m_thread.joinable()) {
            return;
        }
        m_stop_requested.store(true, std::memory_order_release);
        m_thread.join();
    }

    /**
     * @brief Take the clusters collected since the last call, one
     * ClusterVector per frame.
     *
     * Can be called while the collector is running. Clusters that are still
     * on their way are not included: to get those of every frame given to
     * a ClusterFinderMT so far, call its sync() first, from the thread that
     * gives it the frames.
     */
    std::vector<ClusterVector<ClusterType>> steal_clusters() {
        std::vector<ClusterVector<ClusterType>> clusters;
        {
            std::lock_guard<std::mutex> lock(m_clusters_mutex);
            clusters.swap(m_clusters);
        }
        return clusters;
    }
};

} // namespace aare
