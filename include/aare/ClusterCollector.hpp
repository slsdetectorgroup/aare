#pragma once
#include <atomic>
#include <thread>

#include "aare/ClusterFinderMT.hpp"
#include "aare/ClusterVector.hpp"
#include "aare/ProducerConsumerQueue.hpp"

namespace aare {

template <typename ClusterType,
          typename = std::enable_if_t<is_cluster_v<ClusterType>>>
class ClusterCollector {
    ProducerConsumerQueue<ClusterVector<ClusterType>> *m_source;
    std::atomic<bool> m_stop_requested{false};
    std::atomic<bool> m_stopped{true};
    std::chrono::milliseconds m_default_wait{1};
    std::thread m_thread;
    std::vector<ClusterVector<ClusterType>> m_clusters;

    void process() {
        m_stopped = false;
        fmt::print("ClusterCollector started\n");
        while (!m_stop_requested || !m_source->isEmpty()) {
            if (ClusterVector<ClusterType> *clusters = m_source->frontPtr();
                clusters != nullptr) {
                m_clusters.push_back(std::move(*clusters));
                m_source->popFront();
            } else {
                std::this_thread::sleep_for(m_default_wait);
            }
        }
        fmt::print("ClusterCollector stopped\n");
        m_stopped = true;
    }

  public:
    ClusterCollector(ClusterFinderMT<ClusterType, uint16_t, double> *source) {
        m_source = source->sink();
        m_thread =
            std::thread(&ClusterCollector::process,
                        this); // only one process does that so why isnt it
                               // automatically written to m_cluster in collect
                               // - instead of writing first to m_sink?
    }
    void stop() {
        m_stop_requested = true;
        m_thread.join();
    }
    std::vector<ClusterVector<ClusterType>> steal_clusters() {
        if (!m_stopped) {
            throw std::runtime_error("ClusterCollector is still running");
        }
        return std::move(m_clusters);
    }
};

} // namespace aare