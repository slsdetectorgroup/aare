// SPDX-License-Identifier: MPL-2.0
#pragma once
#include <atomic>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <thread>

#include <fmt/format.h>

#include "aare/ClusterVector.hpp"
#include "aare/ProducerConsumerQueue.hpp"
#include "aare/cf/ClusterFinderMT.hpp"
#include "aare/logger.hpp"

namespace aare {

/**
 * @brief Takes the clusters out of a sink queue, such as the one of a
 * ClusterFinderMT, on its own thread and writes them to a binary cluster
 * file that ClusterFile reads.
 *
 * The thread is started by the constructor and stopped by stop() or the
 * destructor, which also close the file. A stopped sink cannot be started
 * again.
 *
 * The queue must outlive the sink.
 */
template <typename ClusterType,
          typename = std::enable_if_t<is_cluster_v<ClusterType>>,
          typename = std::enable_if_t<no_2x2_cluster<ClusterType>::value>>
class ClusterFileSink {
    using SourceQueue = ProducerConsumerQueue<ClusterVector<ClusterType>>;

    SourceQueue *m_source;
    std::atomic<bool> m_stop_requested{false};
    std::chrono::milliseconds m_default_wait{1};
    std::ofstream m_file;
    std::thread m_thread;

    void process() {
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
    }

  public:
    /**
     * @brief Open the file, replacing an existing one, and start writing the
     * clusters of the queue to it.
     * @throws std::runtime_error if the file cannot be opened.
     */
    ClusterFileSink(SourceQueue *source, const std::filesystem::path &fname)
        : m_source(source) {
        LOG(logDEBUG) << "ClusterFileSink: "
                      << "source: " << source << ", file: " << fname.string();
        // The file is open before the thread starts: the queue can already
        // hold clusters, which the thread writes at once
        m_file.open(fname, std::ios::binary);
        if (!m_file.is_open()) {
            throw std::runtime_error(
                fmt::format("Could not open: {} for writing", fname.string()));
        }
        m_thread = std::thread(&ClusterFileSink::process, this);
    }

    /// As for a queue, with the sink queue of the cluster finder
    ClusterFileSink(ClusterFinderMT<ClusterType, uint16_t, double> *source,
                    const std::filesystem::path &fname)
        : ClusterFileSink(source->sink(), fname) {}

    /// Stops the thread and closes the file, see stop()
    ~ClusterFileSink() { stop(); }

    // The thread holds a pointer to this object
    ClusterFileSink(const ClusterFileSink &) = delete;
    ClusterFileSink &operator=(const ClusterFileSink &) = delete;

    /**
     * @brief Stop the thread once the queue is empty and close the file.
     * Does nothing if the sink is stopped.
     */
    void stop() {
        if (!m_thread.joinable()) {
            return;
        }
        m_stop_requested = true;
        m_thread.join();
        m_file.close();
    }
};

} // namespace aare
