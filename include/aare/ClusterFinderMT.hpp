// SPDX-License-Identifier: MPL-2.0
#pragma once
#include <atomic>
#include <cstdint>
#include <memory>
#include <thread>
#include <vector>

#include "aare/Backoff.hpp"
#include "aare/CircularFifo.hpp"
#include "aare/ClusterFinder.hpp"
#include "aare/NDArray.hpp"
#include "aare/ProducerConsumerQueue.hpp"
#include "aare/logger.hpp"
#include <ctime>
namespace aare {

enum class FrameType {
    DATA,
    PEDESTAL,
};

/**
 * @brief Ticket identifying a frame buffer in a FramePool. Trivially
 * copyable, the buffer itself never travels through the queues.
 */
struct FrameRef {
    FrameType type{};
    uint32_t slot{};
    uint64_t frame_number{};
};

/**
 * @brief Fixed set of frame buffers allocated once at construction.
 *
 * Buffers are addressed by slot index and are never moved or reallocated, so
 * the queues only need to pass a FrameRef around. This keeps the per frame
 * allocation out of the hot path entirely.
 */
class FramePool {
    std::vector<NDArray<uint16_t, 2>> m_buffers;

  public:
    FramePool(size_t depth, Shape<2> shape) {
        m_buffers.reserve(depth); // no reallocation, slots stay stable
        for (size_t i = 0; i < depth; ++i) {
            m_buffers.emplace_back(shape);
        }
    }

    NDArray<uint16_t, 2> &operator[](uint32_t slot) { return m_buffers[slot]; }
    const NDArray<uint16_t, 2> &operator[](uint32_t slot) const {
        return m_buffers[slot];
    }

    size_t size() const { return m_buffers.size(); }
};

/**
 * @brief ClusterFinderMT is a multi-threaded version of ClusterFinder. It uses
 * a producer-consumer queue to distribute the frames to the threads. The
 * clusters are collected in a single output queue.
 * @tparam FRAME_TYPE type of the frame data
 * @tparam PEDESTAL_TYPE type of the pedestal data
 * @tparam CT type of the cluster data
 */
template <typename ClusterType = Cluster<int32_t, 3, 3>,
          typename FRAME_TYPE = uint16_t, typename PEDESTAL_TYPE = double,
          typename = std::enable_if_t<no_2x2_cluster<ClusterType>::value>>
class ClusterFinderMT {

  protected:
    using CT = typename ClusterType::value_type;
    size_t m_current_thread{0};
    size_t m_n_threads{0};
    using Finder = ClusterFinder<ClusterType, FRAME_TYPE, PEDESTAL_TYPE>;
    using InputQueue = CircularFifo<FrameRef>;
    using OutputQueue = ProducerConsumerQueue<ClusterVector<ClusterType>>;
    std::vector<std::unique_ptr<InputQueue>> m_input_queues;
    std::vector<std::unique_ptr<OutputQueue>> m_output_queues;
    std::vector<std::unique_ptr<FramePool>> m_frame_pools;

    OutputQueue m_sink{1000}; // All clusters go into this queue

    std::vector<std::unique_ptr<Finder>> m_cluster_finders;
    std::vector<std::thread> m_threads;
    std::thread m_collect_thread;
    std::chrono::microseconds m_default_wait{50};

  private:
    std::atomic<bool> m_stop_requested{false};
    std::atomic<bool> m_processing_threads_stopped{true};

    /**
     * @brief Function called by the processing threads. It reads the frames
     * from the input queue and processes them.
     */
    void process(int thread_id) {
        auto cf = m_cluster_finders[thread_id].get();
        auto q = m_input_queues[thread_id].get();
        auto *pool = m_frame_pools[thread_id].get();
        Backoff backoff;

        while (!m_stop_requested || !q->isEmpty()) {
            if (FrameRef *ref = q->frontPtr(); ref != nullptr) {
                backoff.reset();
                auto view = (*pool)[ref->slot].view();

                switch (ref->type) {
                case FrameType::DATA: {
                    cf->find_clusters(view, ref->frame_number);
                    // Steal before the write so a failed write cannot drop the
                    // clusters by re-stealing an empty vector on retry.
                    auto clusters = cf->steal_clusters(true);
                    while (!m_output_queues[thread_id]->write(
                        std::move(clusters))) {
                        backoff.pause();
                    }
                    break;
                }
                case FrameType::PEDESTAL:
                    m_cluster_finders[thread_id]->push_pedestal_frame(view);
                    break;
                }

                // frame is processed, hand the buffer back to the free list
                q->recycle_front();
            } else {
                backoff.pause();
            }
        }
    }

    /**
     * @brief Collect all the clusters from the output queues and write them to
     * the sink
     */
    void collect() {
        bool empty = true;
        Backoff backoff;
        while (!m_stop_requested || !empty || !m_processing_threads_stopped) {
            bool moved_any = false;
            for (auto &queue : m_output_queues) {
                while (auto *front = queue->frontPtr()) {
                    while (!m_sink.write(std::move(*front))) {
                        backoff.pause();
                    }
                    queue->popFront();
                    moved_any = true;
                }
            }
            empty = !moved_any;
            if (moved_any) {
                backoff.reset();
            } else {
                backoff.pause();
            }
        }
    }

  public:
    /**
     * @brief Construct a new ClusterFinderMT object
     * @param image_size size of the image
     * @param cluster_size size of the cluster
     * @param nSigma number of sigma above the pedestal to consider a photon
     * @param capacity initial capacity of the cluster vector. Should match
     * expected number of clusters in a frame per frame.
     * @param min_pedestal_samples minimum number of pedestal frames to
     * accumulate to get reasonable statistics
     * @param n_threads number of threads to use
     * @param queue_depth number of frame buffers per thread. These are
     * allocated once and recycled, so the total resident frame memory is
     * n_threads * queue_depth * frame size. Keeping the in flight data below
     * the L3 size keeps the per frame copy cheap.
     */
    ClusterFinderMT(Shape<2> image_size, PEDESTAL_TYPE nSigma = 5.0,
                    size_t capacity = 2000, size_t min_pedestal_samples = 1000,
                    size_t n_threads = 3, size_t queue_depth = 16)
        : m_n_threads(n_threads) {

        LOG(logDEBUG1) << "ClusterFinderMT: "
                       << "image_size: " << image_size[0] << "x"
                       << image_size[1] << ", nSigma: " << nSigma
                       << ", capacity: " << capacity
                       << ", min_pedestal_samples: " << min_pedestal_samples
                       << ", n_threads: " << n_threads
                       << ", queue_depth: " << queue_depth;

        for (size_t i = 0; i < n_threads; i++) {
            m_cluster_finders.push_back(
                std::make_unique<
                    ClusterFinder<ClusterType, FRAME_TYPE, PEDESTAL_TYPE>>(
                    image_size, nSigma, capacity, min_pedestal_samples));
        }
        for (size_t i = 0; i < n_threads; i++) {
            m_frame_pools.emplace_back(
                std::make_unique<FramePool>(queue_depth, image_size));
            m_input_queues.emplace_back(std::make_unique<InputQueue>(
                static_cast<uint32_t>(queue_depth), [](size_t slot) {
                    return FrameRef{FrameType::DATA,
                                    static_cast<uint32_t>(slot), 0};
                }));
            m_output_queues.emplace_back(std::make_unique<OutputQueue>(200));
        }
        // TODO! Should we start automatically?
        start();
    }

    /**
     * @brief Return the sink queue where all the clusters are collected
     * @warning You need to empty this queue otherwise the cluster finder will
     * wait forever
     */
    ProducerConsumerQueue<ClusterVector<ClusterType>> *sink() {
        return &m_sink;
    }

    /**
     * @brief Start all processing threads
     */
    void start() {
        m_processing_threads_stopped = false;
        m_stop_requested = false;

        for (size_t i = 0; i < m_n_threads; i++) {
            m_threads.push_back(
                std::thread(&ClusterFinderMT::process, this, i));
        }

        m_collect_thread = std::thread(&ClusterFinderMT::collect, this);
    }

    /**
     * @brief Stop all processing threads
     */
    void stop() {
        m_stop_requested = true;

        for (auto &thread : m_threads) {
            thread.join();
        }
        m_threads.clear();

        m_processing_threads_stopped = true;
        m_collect_thread.join();
    }

    /**
     * @brief Wait for all the queues to be empty. Mostly used for timing tests.
     */
    void sync() {
        for (auto &q : m_input_queues) {
            while (!q->isEmpty()) {
                std::this_thread::sleep_for(m_default_wait);
            }
        }
        for (auto &q : m_output_queues) {
            while (!q->isEmpty()) {
                std::this_thread::sleep_for(m_default_wait);
            }
        }
        while (!m_sink.isEmpty()) {
            std::this_thread::sleep_for(m_default_wait);
        }
    }

    /**
     * @brief Push a pedestal frame to all the cluster finders. The frames is
     * expected to be dark. No photon finding is done. Just pedestal update.
     */
    void push_pedestal_frame(NDView<FRAME_TYPE, 2> frame) {
        for (size_t i = 0; i < m_n_threads; ++i) {
            auto *q = m_input_queues[i].get();
            Backoff backoff;

            FrameRef ref;
            while (!q->try_pop_free(ref)) {
                backoff.pause();
            }

            ref.type = FrameType::PEDESTAL;
            ref.frame_number = 0;
            (*m_frame_pools[i])[ref.slot].copy_from(frame);

            // Cannot fail, the free list is what limits how many frames are
            // in flight so there is always room in the filled list.
            q->try_push_value(ref);
        }
    }

    /**
     * @brief Push the frame to the queue of the next available thread. Function
     * returns once the frame is in a queue.
     * @note Spin locks with a default wait if the queue is full.
     */
    void find_clusters(NDView<FRAME_TYPE, 2> frame, uint64_t frame_number = 0) {
        const size_t tid = m_current_thread % m_n_threads;
        auto *q = m_input_queues[tid].get();
        Backoff backoff;

        FrameRef ref;
        while (!q->try_pop_free(ref)) {
            backoff.pause();
        }

        ref.type = FrameType::DATA;
        ref.frame_number = frame_number;

        // DualTimer dt;
        (*m_frame_pools[tid])[ref.slot].copy_from(frame);
        // auto [wall_ns, cpu_ns] = dt.elapsed();
        // std::cerr << "ClusterFinderMT: find_clusters: copied frame "
        //           << frame_number << " took " << wall_ns/1000.0 << " wall_us
        //           and "
        //           << cpu_ns/1000.0 << " cpu_us" << std::endl;

        // Cannot fail, the free list is what limits how many frames are in
        // flight so there is always room in the filled list.
        q->try_push_value(ref);
        m_current_thread++;
    }

    void clear_pedestal() {
        if (!m_processing_threads_stopped) {
            throw std::runtime_error("ClusterFinderMT is still running");
        }
        for (auto &cf : m_cluster_finders) {
            cf->clear_pedestal();
        }
    }

    /**
     * @brief Recompute the threshold (nSigma * pedestal std) on all cluster
     * finders. Requires the processing threads to be stopped.
     */
    void update_threshold() {
        if (!m_processing_threads_stopped) {
            throw std::runtime_error("ClusterFinderMT is still running");
        }
        for (auto &cf : m_cluster_finders) {
            cf->update_threshold();
        }
    }

    /**
     * @brief Return the pedestal currently used by the cluster finder
     * @param thread_index index of the thread
     */
    auto pedestal(size_t thread_index = 0) {
        if (m_cluster_finders.empty()) {
            throw std::runtime_error("No cluster finders available");
        }
        if (!m_processing_threads_stopped) {
            throw std::runtime_error("ClusterFinderMT is still running");
        }
        if (thread_index >= m_cluster_finders.size()) {
            throw std::runtime_error("Thread index out of range");
        }
        return m_cluster_finders[thread_index]->pedestal();
    }

    /**
     * @brief Return the noise currently used by the cluster finder
     * @param thread_index index of the thread
     */
    auto noise(size_t thread_index = 0) {
        if (m_cluster_finders.empty()) {
            throw std::runtime_error("No cluster finders available");
        }
        if (!m_processing_threads_stopped) {
            throw std::runtime_error("ClusterFinderMT is still running");
        }
        if (thread_index >= m_cluster_finders.size()) {
            throw std::runtime_error("Thread index out of range");
        }
        return m_cluster_finders[thread_index]->noise();
    }

    // void push(FrameWrapper&& frame) {
    //     //TODO! need to loop until we are successful
    //     auto rc = m_input_queue.write(std::move(frame));
    //     fmt::print("pushed frame {}\n", rc);
    // }

    /**
     * @brief Set the nSigma value for all the cluster finders.
     * @param nSigma number of sigma above the pedestal to consider a photon
     * during cluster finding.
     */
    void set_nSigma(const PEDESTAL_TYPE nSigma) {
        // Wait for all queues to be empty before changing the sigma
        for (auto &q : m_input_queues) {
            while (!q->isEmpty()) {
                std::this_thread::sleep_for(m_default_wait);
            }
        }
        for (auto &cf : m_cluster_finders) {
            cf->set_nSigma(nSigma);
        }
    }
};

} // namespace aare