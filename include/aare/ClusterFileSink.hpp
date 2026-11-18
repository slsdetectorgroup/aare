// SPDX-License-Identifier: MPL-2.0
#pragma once
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <type_traits>
#include <vector>

#include "aare/ClusterFinderImpl.hpp" // no_2x2_cluster
#include "aare/ClusterSink.hpp"
#include "aare/ClusterVector.hpp"
#include "aare/ProducerConsumerQueue.hpp"
#include "aare/logger.hpp"

namespace aare {

/**
 * @brief ClusterSink that writes every ClusterVector to a binary cluster file
 * (int32 frame_number, uint32 n_clusters, raw cluster bytes).
 */
template <typename ClusterType,
          typename = std::enable_if_t<is_cluster_v<ClusterType>>,
          typename = std::enable_if_t<no_2x2_cluster<ClusterType>::value>>
class ClusterFileSink
    : public ClusterSink<ClusterFileSink<ClusterType>, ClusterType> {
    using Base = ClusterSink<ClusterFileSink<ClusterType>, ClusterType>;
    friend Base;

  public:
    using SourceQueue = typename Base::SourceQueue;

    /** @brief Open fname for writing, start it with collect(). */
    explicit ClusterFileSink(const std::filesystem::path &fname)
        : m_file(fname, std::ios::binary) {
        if (!m_file) {
            throw std::runtime_error("ClusterFileSink: could not open " +
                                     fname.string());
        }
        LOG(logDEBUG) << "ClusterFileSink: file: " << fname.string();
    }

    ~ClusterFileSink() { this->stop(); }

    void stop() {
        Base::stop();
        if (m_file.is_open()) {
            m_file.close();
        }
    }

  private:
    std::ofstream m_file;

    void consume(ClusterVector<ClusterType> &&clusters) {
        // TODO! Should we store frame number already as int?
        int32_t frame_number = clusters.frame_number();
        uint32_t num_clusters = clusters.size();
        m_file.write(reinterpret_cast<const char *>(&frame_number),
                     sizeof(frame_number));
        m_file.write(reinterpret_cast<const char *>(&num_clusters),
                     sizeof(num_clusters));
        m_file.write(reinterpret_cast<const char *>(clusters.data()),
                     clusters.size() * clusters.item_size());
    }

    std::vector<ClusterVector<ClusterType>> steal_clusters_impl() {
        LOG(logWARNING) << "ClusterFileSink::steal_clusters: clusters are "
                           "written to file, returning an empty vector";
        return {};
    }
};

} // namespace aare
