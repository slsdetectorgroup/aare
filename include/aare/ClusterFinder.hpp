// SPDX-License-Identifier: MPL-2.0
#pragma once
#include <cstddef>
#include <cstdint>
#include <memory>
#include <type_traits>
#include <utility>

#include "aare/ClusterFinderMT.hpp"
#include "aare/ClusterSink.hpp"
#include "aare/NDView.hpp"

namespace aare {

/**
 * @brief Base class for a multithreaded finder that feeds its clusters into a
 * sink.
 *
 * Owns a ClusterFinderMT and a SinkType (for example ClusterCollector or
 * ClusterFileSink). Both are started on construction: the sink's collect()
 * spawns the thread that drains the finder's output queue. stop() processes
 * all queued frames, then drains and stops the sink. The destructor calls
 * stop().
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
     * @param queue_depth number of frame buffers per thread. These are
     * allocated once and recycled, so the total resident frame memory is
     * n_threads * queue_depth * frame size.
     * @param min_pedestal_samples minimum number of pedestal samples to
     * accumulate before using the pedestal
     */
    explicit ClusterFinder(Shape<2> image_size, PEDESTAL_TYPE nSigma = 5.0,
                           size_t capacity = 2000, size_t n_threads = 3,
                           size_t queue_depth = 16,
                           size_t min_pedestal_samples = 1000)
        : m_sink(std::make_shared<SinkType>()),
          m_finder(image_size, nSigma, capacity, n_threads, queue_depth,
                   min_pedestal_samples) {
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
        : m_sink(
              std::make_shared<SinkType>(std::forward<SinkArgs>(sink_args)...)),
          m_finder(image_size, nSigma, capacity, n_threads, queue_depth,
                   min_pedestal_samples) {
        connect_sink();
    }

    // The sink thread holds a pointer to the finder's output queue
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
        // The finder flushes into its output queue, so the sink has to keep
        // draining until the finder threads are joined.
        m_finder.stop();
        m_sink->stop();
        m_running = false;
    }

    /**
     * @brief Wait until all queued frames have been processed and handed to
     * the sink.
     */
    void sync() { m_finder.sync(); }

    /**
     * @brief Push a pedestal frame to all the cluster finders. The frame is
     * expected to be dark. No photon finding is done, just pedestal update.
     */
    void push_pedestal_frame(NDView<FRAME_TYPE, 2> frame) {
        m_finder.push_pedestal_frame(frame);
    }

    /**
     * @brief Push the frame to the queue of the next thread. Returns once the
     * frame is in a queue.
     */
    void find_clusters(NDView<FRAME_TYPE, 2> frame, uint64_t frame_number = 0) {
        m_finder.find_clusters(frame, frame_number);
    }

    std::shared_ptr<SinkType> sink() { return m_sink; }
    std::shared_ptr<const SinkType> sink() const { return m_sink; }

    /**
     * @brief Reset the pedestal of all cluster finders. Requires the
     * processing threads to be stopped.
     */
    void clear_pedestal() { m_finder.clear_pedestal(); }

    /**
     * @brief Recompute the threshold (nSigma * pedestal std) on all cluster
     * finders. Requires the processing threads to be stopped.
     */
    void update_threshold() { m_finder.update_threshold(); }

    /**
     * @brief Set the nSigma value for all the cluster finders. Waits for the
     * queued frames to be processed first.
     */
    void set_nSigma(PEDESTAL_TYPE nSigma) { m_finder.set_nSigma(nSigma); }

    /**
     * @brief Return whether all cluster finders have received enough pedestal
     * frames to find clusters. Waits for the queued frames to be processed
     * first.
     */
    bool pedestal_ready() { return m_finder.pedestal_ready(); }

    /**
     * @brief Return the pedestal currently used by one cluster finder.
     * Requires the processing threads to be stopped.
     */
    auto pedestal(size_t thread_index = 0) {
        return m_finder.pedestal(thread_index);
    }

    /**
     * @brief Return the noise currently used by one cluster finder.
     * Requires the processing threads to be stopped.
     */
    auto noise(size_t thread_index = 0) { return m_finder.noise(thread_index); }

    /**
     * @brief Return whether the finder and sink are still running.
     */
    bool running() const { return m_running; }

  protected:
    // The sink is declared first so it is constructed before the finder
    // starts its threads: a throwing sink constructor (for example a file
    // that cannot be opened) then leaves no running thread behind.
    std::shared_ptr<SinkType> m_sink;

  private:
    void connect_sink() {
        m_sink->collect(m_finder.sink());
        m_running = true;
    }

    Finder m_finder;
    bool m_running{false};
};

} // namespace aare
