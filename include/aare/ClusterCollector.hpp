#pragma once
#include <atomic>
#include <thread>

#include "aare/ProducerConsumerQueue.hpp"
#include "aare/ClusterVector.hpp"
#include "aare/ClusterFinderMT.hpp"

namespace aare {

class ClusterCollector{
        ProducerConsumerQueue<ClusterVector<int>>* m_source;
        std::atomic<bool> m_stop_requested{false};
        std::atomic<bool> m_stopped{true};
        std::chrono::milliseconds m_default_wait{1};
        std::thread m_thread;
        std::vector<ClusterVector<int>> m_clusters;

    void process(){
        m_stopped = false;
        fmt::print("ClusterCollector started\n");
        while (!m_stop_requested || !m_source->isEmpty()) { 
            if (ClusterVector<int> *clusters = m_source->frontPtr();
                clusters != nullptr) {
                m_clusters.push_back(std::move(*clusters));
                m_source->popFront();
            }else{
                std::this_thread::sleep_for(m_default_wait);
            }
        }
        fmt::print("ClusterCollector stopped\n");
        m_stopped = true;
    }

    public:
        ClusterCollector(ClusterFinderMT<uint16_t, double, int32_t>* source){
            m_source = source->sink();
            m_thread = std::thread(&ClusterCollector::process, this);
        }
        void stop(){
            m_stop_requested = true;
            m_thread.join();
        }
        std::vector<ClusterVector<int>> steal_clusters(){
            if(!m_stopped){
                throw std::runtime_error("ClusterCollector is still running");
            }
            return std::move(m_clusters);
        }
};

} // namespace aare