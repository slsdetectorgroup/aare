
#include "aare/file_io.hpp"
#include "aare/processing/ClusterFinder.hpp"
#include "aare/processing/Pedestal.hpp"

#include <chrono>
#include <fmt/format.h>
using namespace std::chrono;
#include "aare/core/ProducerConsumerQueue.hpp"
#include <memory>
#include <thread>

using Queue = folly::ProducerConsumerQueue<aare::Frame>;

//Global constants, for easy tweaking
constexpr size_t print_interval = 100;
constexpr std::chrono::milliseconds default_wait(1);
constexpr uint32_t queue_size = 1000;

// Small wrapper to do cluster finding in a thread
// helps with keeping track of stopping tokens etc.
class ThreadedClusterFinder {
    std::atomic<bool> m_stop_requested = false;
    std::atomic<size_t> m_frames_processed = 0;
    Queue *m_queue = nullptr;
    aare::Pedestal<double> m_pedestal;
    int m_object_id;

  public:
    ThreadedClusterFinder(Queue &q, aare::Pedestal<double> pd, int id) : m_queue(&q), m_pedestal(pd), m_object_id(id) {}

    size_t frames_processed() const { return m_frames_processed; }
    void request_stop() {
        fmt::print("{}:Stop requested\n", m_object_id);
        m_stop_requested = true;
    }

    void find_clusters() {
        aare::ClusterFinder cf(3, 3, 5, 0);
        while (!m_stop_requested) {
            aare::Frame frame(1, 1, aare::Dtype("u4"));
            if (m_queue->read(frame)) {
                auto clusters = cf.find_clusters_without_threshold(frame.view<uint16_t>(), m_pedestal, false);
                m_frames_processed++;
                if (m_frames_processed % print_interval == 0) {
                    fmt::print("{}:Found {} clusters\n", m_object_id, clusters.size());
                }
            } else {
                fmt::print("{}:Queue empty\n", m_object_id);
                std::this_thread::sleep_for(default_wait);
            }
        }
        fmt::print("{}:Done\n", m_object_id);
    }
};

int main(int argc, char **argv) {
    //Rudimentary argument parsing
    if (argc != 3) {
        fmt::print("Usage: {} <file> <n_threads>\n", argv[0]);
        return 1;
    }

    std::filesystem::path fname(argv[1]);
    fmt::print("Loading {}\n", fname.c_str());
    const int n_threads = std::stoi(argv[2]);

    aare::Pedestal<double> pd(400, 400, 1000);
    aare::File f(fname, "r");

    // Use the first 1000 frames to calculate the pedestal
    // we can then copy this pedestal to each thread
    auto t0 = high_resolution_clock::now();
    for (int i = 0; i < 1000; ++i) {
        aare::Frame frame = f.iread(i);
        pd.push<uint16_t>(frame);
    }
    auto t1 = high_resolution_clock::now();
    fmt::print("Pedestal run took: {}s\n", duration_cast<microseconds>(t1 - t0).count() / 1e6);

    //---------------------------------------------------------------------------------------------
    //---------------------- Now lets start with the setup for the threaded cluster finding

    // We need one queue per thread...
    std::vector<Queue> queues;
    for (int i = 0; i < n_threads; ++i) {
        queues.emplace_back(queue_size);
    }

    // and also one cluster finder per thread
    std::vector<std::unique_ptr<ThreadedClusterFinder>> cluster_finders;
    for (int i = 0; i < n_threads; ++i) {
        cluster_finders.push_back(std::make_unique<ThreadedClusterFinder>(queues[i], pd, i));
    }

    // next we start the threads
    std::vector<std::thread> threads;
    for (int i = 0; i < n_threads; ++i) {
        threads.emplace_back(&ThreadedClusterFinder::find_clusters, cluster_finders[i].get());
    }

    // Push frames to the queues
    const int n_frames = 1000;
    for (int i = 0; i < n_frames; ++i) {
        // if the Queue is full, wait, there are better ways to do this =)
        while (queues[i % n_threads].isFull()) {
            fmt::print("Queue {} is full, waiting\n", i % n_threads);
            std::this_thread::sleep_for(default_wait);
        }
        queues[i % n_threads].write(f.iread(i+1000));
        if (i % 100 == 0) {
            fmt::print("Pushed frame {}\n", i);
        }
    }


    // wait for all queues to be empty
    for (auto &q : queues) {
        while (!q.isEmpty()) {
            // fmt::print("Finish Queue not empty, waiting\n");
            std::this_thread::sleep_for(default_wait);
        }
    }

    // and once empty we stop the cluster finders
    for (auto &cf : cluster_finders) {
        cf->request_stop();
    }
    for (auto &t : threads) {
        t.join();
    }

    size_t total_frames = 0;
    for (auto &cf : cluster_finders) {
        total_frames += cf->frames_processed();
    }
    auto t2 = high_resolution_clock::now();
    fmt::print("Processed {} frames in {}s\n", total_frames, duration_cast<microseconds>(t2 - t1).count() / 1e6);

    // auto start = high_resolution_clock::now();
    // aare::ClusterFinder cf(3, 3, 5, 0);
    // for (int i = 1000; i<2000; ++i){
    //     aare::Frame frame = f.iread(i);
    //     auto clusters = cf.find_clusters_without_threshold(frame.view<uint16_t>(), pd, true);
    // }

    // auto stop = high_resolution_clock::now();
    // auto duration = duration_cast<microseconds>(stop - start);
    // fmt::print("Run took: {}s\n", duration.count()/1e6);
}