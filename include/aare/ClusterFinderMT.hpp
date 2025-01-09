#pragma once
#include <atomic>
#include <cstdint>
#include <memory>
#include <thread>
#include <vector>

#include "aare/NDArray.hpp"
#include "aare/ProducerConsumerQueue.hpp"
#include "aare/ClusterFinder.hpp"

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

    void process(int thread_id) {
        auto cf = m_cluster_finders[thread_id].get();
        auto q = m_input_queues[thread_id].get();
        // TODO! Avoid indexing into the vector every time
        fmt::print("Thread {} started\n", thread_id);
        // TODO! is this check enough to make sure we process all the frames?
        while (!m_stop_requested || !q->isEmpty()) {
            if (FrameWrapper *frame = q->frontPtr(); frame != nullptr) {
                // fmt::print("Thread {} got frame {}, type: {}\n", thread_id,
                //    frame->frame_number, static_cast<int>(frame->type));

                switch (frame->type) {
                case FrameType::DATA:
                    cf->find_clusters(frame->data.view(), frame->frame_number);
                    m_output_queues[thread_id]->write(cf->steal_clusters());

                    break;

                case FrameType::PEDESTAL:
                    m_cluster_finders[thread_id]->push_pedestal_frame(
                        frame->data.view());
                    break;

                default:
                    break;
                }

                // frame is processed now discard it
                m_input_queues[thread_id]->popFront();
            } else {
                std::this_thread::sleep_for(m_default_wait);
            }
        }
        fmt::print("Thread {} stopped\n", thread_id);
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
    ClusterFinderMT(Shape<2> image_size, Shape<2> cluster_size,
                    PEDESTAL_TYPE nSigma = 5.0, size_t capacity = 2000,
                    size_t n_threads = 3)
        : m_n_threads(n_threads) {
        fmt::print("ClusterFinderMT: using {} threads\n", n_threads);
        for (size_t i = 0; i < n_threads; i++) {
            m_cluster_finders.push_back(
                std::make_unique<ClusterFinder<FRAME_TYPE, PEDESTAL_TYPE, CT>>(
                    image_size, cluster_size, nSigma, capacity));
        }
        for (size_t i = 0; i < n_threads; i++) {
            m_input_queues.emplace_back(std::make_unique<InputQueue>(200));
            m_output_queues.emplace_back(std::make_unique<OutputQueue>(200));
        }

        start();
    }

    ProducerConsumerQueue<ClusterVector<int>> *sink() { return &m_sink; }

    /**
     * @brief Start all threads
     */

    void start() {
        for (size_t i = 0; i < m_n_threads; i++) {
            m_threads.push_back(
                std::thread(&ClusterFinderMT::process, this, i));
        }
        m_processing_threads_stopped = false;
        m_collect_thread = std::thread(&ClusterFinderMT::collect, this);
    }

    /**
     * @brief Stop all threads
     */
    void stop() {
        m_stop_requested = true;
        for (auto &thread : m_threads) {
            thread.join();
        }
        m_processing_threads_stopped = true;
        m_collect_thread.join();
    }

    /**
     * @brief Wait for all the queues to be empty
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

    auto pedestal() { 
        if (m_cluster_finders.empty()) {
            throw std::runtime_error("No cluster finders available");
        }
        if(!m_processing_threads_stopped){
            throw std::runtime_error("ClusterFinderMT is still running");
        }
        return m_cluster_finders[0]->pedestal(); 
    }

    auto noise() { 
        if (m_cluster_finders.empty()) {
            throw std::runtime_error("No cluster finders available");
        }
        if(!m_processing_threads_stopped){
            throw std::runtime_error("ClusterFinderMT is still running");
        }
        return m_cluster_finders[0]->noise(); 
    }

    // void push(FrameWrapper&& frame) {
    //     //TODO! need to loop until we are successful
    //     auto rc = m_input_queue.write(std::move(frame));
    //     fmt::print("pushed frame {}\n", rc);
    // }
};

} // namespace aare