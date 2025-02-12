#pragma once
#include <atomic>
#include <cstdint>
#include <memory>
#include <thread>
#include <vector>

#include "aare/ClusterFinder.hpp"
#include "aare/NDArray.hpp"
#include "aare/ProducerConsumerQueue.hpp"

namespace aare {

enum class FrameType {
    DATA,
    PEDESTAL,
};

struct FrameWrapper {
    FrameType type;
    uint64_t frame_number;
    NDArray<uint16_t, 2> data;
};

/**
 * @brief ClusterFinderMT is a multi-threaded version of ClusterFinder. It uses
 * a producer-consumer queue to distribute the frames to the threads. The
 * clusters are collected in a single output queue.
 * @tparam FRAME_TYPE type of the frame data
 * @tparam PEDESTAL_TYPE type of the pedestal data
 * @tparam CT type of the cluster data
 */
template <typename FRAME_TYPE = uint16_t, typename PEDESTAL_TYPE = double,
          typename CT = int32_t>
class ClusterFinderMT {
    size_t m_current_thread{0};
    size_t m_n_threads{0};
    using Finder = ClusterFinder<FRAME_TYPE, PEDESTAL_TYPE, CT>;
    using InputQueue = ProducerConsumerQueue<FrameWrapper>;
    using OutputQueue = ProducerConsumerQueue<ClusterVector<int>>;
    std::vector<std::unique_ptr<InputQueue>> m_input_queues;
    std::vector<std::unique_ptr<OutputQueue>> m_output_queues;

    OutputQueue m_sink{1000}; // All clusters go into this queue

    std::vector<std::unique_ptr<Finder>> m_cluster_finders;
    std::vector<std::thread> m_threads;
    std::thread m_collect_thread;
    std::chrono::milliseconds m_default_wait{1};

    std::atomic<bool> m_stop_requested{false};
    std::atomic<bool> m_processing_threads_stopped{true};

    /**
     * @brief Function called by the processing threads. It reads the frames
     * from the input queue and processes them.
     */
    void process(int thread_id) {
        auto cf = m_cluster_finders[thread_id].get();
        auto q = m_input_queues[thread_id].get();
        bool realloc_same_capacity = true;

        while (!m_stop_requested || !q->isEmpty()) {
            if (FrameWrapper *frame = q->frontPtr(); frame != nullptr) {

                switch (frame->type) {
                case FrameType::DATA:
                    cf->find_clusters(frame->data.view(), frame->frame_number);
                    m_output_queues[thread_id]->write(cf->steal_clusters(realloc_same_capacity));
                    break;

                case FrameType::PEDESTAL:
                    m_cluster_finders[thread_id]->push_pedestal_frame(
                        frame->data.view());
                    break;
                }

                // frame is processed now discard it
                m_input_queues[thread_id]->popFront();
            } else {
                std::this_thread::sleep_for(m_default_wait);
            }
        }
    }

    /**
     * @brief Collect all the clusters from the output queues and write them to
     * the sink
     */
    void collect() {
        bool empty = true;
        while (!m_stop_requested || !empty || !m_processing_threads_stopped) {
            empty = true;
            for (auto &queue : m_output_queues) {
                if (!queue->isEmpty()) {

                    while (!m_sink.write(std::move(*queue->frontPtr()))) {
                        std::this_thread::sleep_for(m_default_wait);
                    }
                    queue->popFront();
                    empty = false;
                }
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
     * @param n_threads number of threads to use
     */
    ClusterFinderMT(Shape<2> image_size, Shape<2> cluster_size,
                    PEDESTAL_TYPE nSigma = 5.0, size_t capacity = 2000,
                    size_t n_threads = 3)
        : m_n_threads(n_threads) {
        for (size_t i = 0; i < n_threads; i++) {
            m_cluster_finders.push_back(
                std::make_unique<ClusterFinder<FRAME_TYPE, PEDESTAL_TYPE, CT>>(
                    image_size, cluster_size, nSigma, capacity));
        }
        for (size_t i = 0; i < n_threads; i++) {
            m_input_queues.emplace_back(std::make_unique<InputQueue>(200));
            m_output_queues.emplace_back(std::make_unique<OutputQueue>(200));
        }
        //TODO! Should we start automatically?
        start();
    }

    /**
     * @brief Return the sink queue where all the clusters are collected
     * @warning You need to empty this queue otherwise the cluster finder will wait forever
     */
    ProducerConsumerQueue<ClusterVector<int>> *sink() { return &m_sink; }

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
        FrameWrapper fw{FrameType::PEDESTAL, 0,
                        NDArray(frame)}; // TODO! copies the data!

        for (auto &queue : m_input_queues) {
            while (!queue->write(fw)) {
                std::this_thread::sleep_for(m_default_wait);
            }
        }
    }

    /**
     * @brief Push the frame to the queue of the next available thread. Function
     * returns once the frame is in a queue.
     * @note Spin locks with a default wait if the queue is full.
     */
    void find_clusters(NDView<FRAME_TYPE, 2> frame, uint64_t frame_number = 0) {
        FrameWrapper fw{FrameType::DATA, frame_number,
                        NDArray(frame)}; // TODO! copies the data!
        while (!m_input_queues[m_current_thread % m_n_threads]->write(fw)) {
            std::this_thread::sleep_for(m_default_wait);
        }
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
};

} // namespace aare