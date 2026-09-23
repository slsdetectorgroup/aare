// SPDX-License-Identifier: MPL-2.0
#pragma once
#include <atomic>
#include <thread>
#include <type_traits>
#include <utility>
#include <vector>

#include "aare/Backoff.hpp"
#include "aare/ClusterFinderMT.hpp"
#include "aare/ClusterVector.hpp"
#include "aare/ProducerConsumerQueue.hpp"
#include "aare/defs.hpp"

namespace aare {

template <typename ClusterType,
          typename = std::enable_if_t<is_cluster_v<ClusterType>>>
class ClusterCollector {
    using SourceQueue = ProducerConsumerQueue<ClusterVector<ClusterType>>;

    SourceQueue *m_source;
    std::atomic<bool> m_stop_requested{false};
    std::atomic<bool> m_stopped{true};
    std::thread m_thread;
    std::vector<ClusterVector<ClusterType>> m_clusters;

    void process() {
        m_stopped.store(false, std::memory_order_release);
        fmt::print("ClusterCollector started\n");
        Backoff backoff;
        while (true) {
            if (ClusterVector<ClusterType> *clusters = m_source->frontPtr();
                clusters != nullptr) {
                backoff.reset();
                m_clusters.push_back(std::move(*clusters));
                m_source->popFront();
                continue;
            }

            if (m_stop_requested.load(std::memory_order_acquire))
                break;

            backoff.pause();
        }
        fmt::print("ClusterCollector stopped\n");
        m_stopped.store(true, std::memory_order_release);
    }

  public:
    explicit ClusterCollector(SourceQueue *source) : m_source(source) {
        m_thread =
            std::thread(&ClusterCollector::process,
                        this); // only one process does that so why isnt it
                               // automatically written to m_cluster in collect
                               // - instead of writing first to m_sink?
    }

    template <typename Finder,
              std::enable_if_t<
                  std::is_same_v<decltype(std::declval<Finder &>().sink()),
                                 SourceQueue *>,
                  int> = 0>
    explicit ClusterCollector(Finder *source)
        : ClusterCollector(source->sink()) {}

    void stop() {
        m_stop_requested.store(true, std::memory_order_release);
        if (m_thread.joinable())
            m_thread.join();
    }
    std::vector<ClusterVector<ClusterType>> steal_clusters() {
        if (!m_stopped.load(std::memory_order_acquire)) {
            throw std::runtime_error("ClusterCollector is still running");
        }
        return std::move(m_clusters);
    }
};

} // namespace aare
