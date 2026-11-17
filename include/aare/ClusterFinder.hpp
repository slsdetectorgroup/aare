// SPDX-License-Identifier: MPL-2.0
#pragma once
#include <cstddef>
#include <cstdint>
#include <type_traits>
#include <utility>
#include <vector>

#include "aare/ClusterFinderMT.hpp"
#include "aare/ClusterSink.hpp"
#include "aare/ClusterVector.hpp"
#include "aare/NDView.hpp"

namespace aare {

/**
 * @brief Multi-threaded cluster finder that feeds its clusters into a sink.
 *
 * Owns a ClusterFinderMT and a SinkType (for example ClusterCollector or
 * ClusterFileSink). Both are started on construction: the sink's collect()
 * spawns the thread that drains the finder's output queue. stop() flushes the
 * finder, then drains and stops the sink. The destructor stops both.
 *
 * @tparam SinkType a class deriving from ClusterSink, its cluster_type sets
 * the cluster type of the finder
 * @tparam FRAME_TYPE type of the frame data
 * @tparam PEDESTAL_TYPE type of the pedestal data
 */
template <typename SinkType, typename FRAME_TYPE = uint16_t,
          typename PEDESTAL_TYPE = double,
          typename = std::enable_if_t<is_cluster_sink_v<SinkType>>>
class ClusterFinder {
  public:
    using ClusterType = typename SinkType::cluster_type;
    using Finder = ClusterFinderMT<ClusterType, FRAME_TYPE, PEDESTAL_TYPE>;

    /**
     * @brief Construct with a default constructed sink.
     * @param image_size size of the image
     * @param nSigma number of sigma above the pedestal to consider a photon
     * @param capacity initial capacity of the cluster vector per frame
     * @param n_threads number of processing threads
     * @param queue_depth number of frame buffers per thread
     * @param min_pedestal_samples minimum number of pedestal samples to
     * accumulate before using the pedestal
     */
    explicit ClusterFinder(Shape<2> image_size, PEDESTAL_TYPE nSigma = 5.0,
                           size_t capacity = 2000, size_t n_threads = 3,
                           size_t queue_depth = 16,
                           size_t min_pedestal_samples = 1000)
        : m_sink(), m_finder(image_size, nSigma, capacity, n_threads,
                             queue_depth, min_pedestal_samples) {
        connect_sink();
    }

    /**
     * @brief Construct the sink from sink_args, for example the file name of
     * a ClusterFileSink. All finder parameters must be given explicitly.
     */
    template <typename... SinkArgs,
              typename = std::enable_if_t<(sizeof...(SinkArgs) > 0)>>
    ClusterFinder(Shape<2> image_size, PEDESTAL_TYPE nSigma, size_t capacity,
                  size_t n_threads, size_t queue_depth,
                  size_t min_pedestal_samples, SinkArgs &&...sink_args)
        : m_sink(std::forward<SinkArgs>(sink_args)...),
          m_finder(image_size, nSigma, capacity, n_threads, queue_depth,
                   min_pedestal_samples) {
        connect_sink();
    }

    ClusterFinder(const ClusterFinder &) = delete;
    ClusterFinder &operator=(const ClusterFinder &) = delete;
    ClusterFinder(ClusterFinder &&) = delete;
    ClusterFinder &operator=(ClusterFinder &&) = delete;

    ~ClusterFinder() { stop(); }

    /**
     * @brief Process all queued frames, then drain and stop the sink.
     * Calling it on a stopped finder is a no-op.
     */
    void stop() {
        if (!m_running) {
            return;
        }
        // The finder flushes into the sink queue, so the sink has to keep
        // draining until the finder threads are joined.
        m_finder.stop();
        m_sink.stop();
        m_running = false;
    }

    /**
     * @brief Wait until all queued frames have been processed and handed to
     * the sink.
     */
    void sync() { m_finder.sync(); }

    void push_pedestal_frame(NDView<FRAME_TYPE, 2> frame) {
        m_finder.push_pedestal_frame(frame);
    }

    void find_clusters(NDView<FRAME_TYPE, 2> frame, uint64_t frame_number = 0) {
        m_finder.find_clusters(frame, frame_number);
    }

    /**
     * @brief Take the clusters held by the sink. Call stop() first.
     * @throws std::runtime_error if the sink is still running
     */
    std::vector<ClusterVector<ClusterType>> steal_clusters() {
        return m_sink.steal_clusters();
    }

    std::vector<ClusterVector<ClusterType>> get_clusters() {
        sync();
        return m_sink.steal_clusters();
    }

    void clear_pedestal() { m_finder.clear_pedestal(); }
    void update_threshold() { m_finder.update_threshold(); }
    void set_nSigma(PEDESTAL_TYPE nSigma) { m_finder.set_nSigma(nSigma); }
    bool pedestal_ready() { return m_finder.pedestal_ready(); }
    auto pedestal(size_t thread_index = 0) {
        return m_finder.pedestal(thread_index);
    }
    auto noise(size_t thread_index = 0) { return m_finder.noise(thread_index); }

    bool running() const { return m_running; }
    SinkType &sink() { return m_sink; }
    const SinkType &sink() const { return m_sink; }

  private:
    void connect_sink() {
        m_sink.collect(m_finder.sink());
        m_running = true;
    }

    // The sink is declared first so it is constructed before the finder
    // starts its threads: a throwing sink constructor (for example a file
    // that cannot be opened) then leaves no running thread behind.
    SinkType m_sink;
    Finder m_finder; // TODO: should be part of the class
    bool m_running{false};
};

} // namespace aare
