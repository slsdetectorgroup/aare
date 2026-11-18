// SPDX-License-Identifier: MPL-2.0
#pragma once
#include <atomic>
#include <stdexcept>
#include <thread>
#include <type_traits>
#include <utility>
#include <vector>

#include "aare/Backoff.hpp"
#include "aare/Cluster.hpp"
#include "aare/ClusterVector.hpp"
#include "aare/ProducerConsumerQueue.hpp"
#include "aare/logger.hpp"

namespace aare {

/**
 * @brief CRTP base for consumers of a ClusterFinderMT sink queue.
 *
 * The base owns the consumer thread and the drain loop. collect() starts the
 * thread, which pops every ClusterVector from the source queue and hands it
 * to the derived class. stop() requests the thread to finish, waits until the
 * queue is drained and joins it.
 *
 * A derived class must provide:
 * - `void consume(ClusterVector<ClusterType> &&clusters)`, called on the
 *   consumer thread for every ClusterVector taken from the queue.
 * - `std::vector<ClusterVector<ClusterType>> steal_clusters_impl()`, called
 *   by steal_clusters() once the consumer thread has stopped.
 *
 * It may provide `void on_stop()`, called by stop() after the thread is
 * joined (for example to close a file). The derived destructor must call
 * stop(), since the consumer thread uses members of the derived class that
 * are already destroyed when the base destructor runs.
 *
 * @tparam Derived the class deriving from ClusterSink
 * @tparam ClusterType type of the clusters in the source queue
 */
template <typename Derived, typename ClusterType,
          typename = std::enable_if_t<is_cluster_v<ClusterType>>>
class ClusterSink {
  public:
    using cluster_type = ClusterType;
    using SourceQueue = ProducerConsumerQueue<ClusterVector<ClusterType>>;

    ClusterSink(const ClusterSink &) = delete;
    ClusterSink &operator=(const ClusterSink &) = delete;
    ClusterSink(ClusterSink &&) = delete;
    ClusterSink &operator=(ClusterSink &&) = delete;

    /**
     * @brief Start the consumer thread reading from source.
     * @throws std::runtime_error if the sink is already collecting
     */
    void collect(SourceQueue *source) {
        if (source == nullptr) {
            throw std::invalid_argument("ClusterSink: source queue is null");
        }
        if (m_thread.joinable()) {
            LOG(logINFO) << "ClusterSink is already collecting";
            return;
        }
        m_source = source;
        m_stop_requested.store(false, std::memory_order_release);
        // Set before spawning so that steal_clusters() called right after
        // collect() cannot observe a stopped sink.
        m_thread = std::thread(&ClusterSink::process, this);
    }

    void stop() {
        if (!m_thread.joinable()) {
            return;
        }
        m_stop_requested.store(true, std::memory_order_release);
        m_thread.join();
    }

    /**
     * @brief Take the clusters held by the sink.
     * @throws std::runtime_error if the consumer thread is still running
     */
    std::vector<ClusterVector<ClusterType>> steal_clusters() {
        return derived().steal_clusters_impl();
    }

  protected:
    ClusterSink() = default;
    // Protected and non-virtual: sinks are never deleted through the base.
    ~ClusterSink() { stop(); }

  private:
    SourceQueue *m_source{nullptr};
    std::atomic<bool> m_stop_requested{false};
    std::thread m_thread;

    Derived &derived() { return static_cast<Derived &>(*this); }

    void consume(ClusterVector<ClusterType> &&clusters) {
        derived().consume(
            std::move(clusters)); // TODO: what if they dont have consume?
    }

    void process() {
        Backoff backoff;
        while (true) {
            if (ClusterVector<ClusterType> *clusters = m_source->frontPtr();
                clusters != nullptr) {
                backoff.reset();
                derived().consume(std::move(*clusters));
                m_source->popFront();
                continue;
            }
            // Only leave once the queue is empty so no cluster is dropped.
            if (m_stop_requested.load(std::memory_order_acquire)) {
                break;
            }
            backoff.pause();
        }
        LOG(logINFO) << "ClusterSink stopped";
    }
};

/**
 * @brief True if T derives from ClusterSink<T, typename T::cluster_type>.
 */
template <typename T, typename = void>
struct is_cluster_sink : std::false_type {};

template <typename T>
struct is_cluster_sink<T, std::void_t<typename T::cluster_type>>
    : std::is_base_of<ClusterSink<T, typename T::cluster_type>, T> {};

template <typename T>
inline constexpr bool is_cluster_sink_v = is_cluster_sink<T>::value;

} // namespace aare
