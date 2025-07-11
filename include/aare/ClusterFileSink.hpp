#pragma once
#include <atomic>
#include <filesystem>
#include <thread>

#include "aare/ClusterFinderMT.hpp"
#include "aare/ClusterVector.hpp"
#include "aare/ProducerConsumerQueue.hpp"

namespace aare {

template <typename ClusterType,
          typename = std::enable_if_t<is_cluster_v<ClusterType>>>
class ClusterFileSink {
    ProducerConsumerQueue<ClusterVector<ClusterType>> *m_source;
    std::atomic<bool> m_stop_requested{false};
    std::atomic<bool> m_stopped{true};
    std::chrono::milliseconds m_default_wait{1};
    std::thread m_thread;
    std::ofstream m_file;

    void process() {
        m_stopped = false;
        LOG(logDEBUG) << "ClusterFileSink started";
        while (!m_stop_requested || !m_source->isEmpty()) {
            if (ClusterVector<ClusterType> *clusters = m_source->frontPtr();
                clusters != nullptr) {
                // Write clusters to file
                int32_t frame_number =
                    clusters->frame_number(); // TODO! Should we store frame
                                              // number already as int?
                uint32_t num_clusters = clusters->size();
                m_file.write(reinterpret_cast<const char *>(&frame_number),
                             sizeof(frame_number));
                m_file.write(reinterpret_cast<const char *>(&num_clusters),
                             sizeof(num_clusters));
                m_file.write(reinterpret_cast<const char *>(clusters->data()),
                             clusters->size() * clusters->item_size());
                m_source->popFront();
            } else {
                std::this_thread::sleep_for(m_default_wait);
            }
        }
        LOG(logDEBUG) << "ClusterFileSink stopped";
        m_stopped = true;
    }

  public:
    ClusterFileSink(ClusterFinderMT<ClusterType, uint16_t, double> *source,
                    const std::filesystem::path &fname) {
        LOG(logDEBUG) << "ClusterFileSink: "
                      << "source: " << source->sink()
                      << ", file: " << fname.string();
        m_source = source->sink();
        m_thread = std::thread(&ClusterFileSink::process, this);
        m_file.open(fname, std::ios::binary);
    }
    void stop() {
        m_stop_requested = true;
        m_thread.join();
        m_file.close();
    }
};

} // namespace aare