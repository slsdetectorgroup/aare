#pragma once
#include "aare/NDArray.hpp"
#include "aare/NDView.hpp"
#include "aare/ProducerConsumerQueue.hpp"

//Lets see if we need to hide it behind a pimpl
#include <boost/histogram.hpp>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>
namespace bh = boost::histogram;

namespace aare {
class PixelHistogram {
  public:
    using StorageType = uint16_t;
    using AxisType = float;

  private:
    using Axes = std::tuple<
        bh::axis::regular<AxisType, bh::use_default, bh::use_default,
                          bh::axis::option::none_t>,
        bh::axis::integer<int, bh::use_default, bh::axis::option::none_t>,
        bh::axis::integer<int, bh::use_default, bh::axis::option::none_t>>;
    using Hist = bh::histogram<Axes, bh::dense_storage<StorageType>>;
    using AsyncQueue = ProducerConsumerQueue<NDArray<AxisType, 2>>;

    int rows_;
    int cols_;
    int n_threads_;
    const AxisType xmin_;
    const AxisType xmax_;
    std::vector<Hist> partial_hists_;
    // Cumulative row offsets so that thread t owns rows
    //     [row_offsets_[t], row_offsets_[t + 1])
    // Length is n_threads_ + 1; partition is balanced (the first
    // rows_ % n_threads_ threads get one extra row each).
    std::vector<int> row_offsets_;

    // Thread pool members
    std::vector<std::thread> workers_;
    std::mutex work_mutex_;
    std::condition_variable work_cv_;
    std::condition_variable done_cv_;
    const NDView<AxisType, 2>* current_image_;
    std::atomic<int> completed_threads_;
    std::atomic<bool> stop_workers_;
    int work_generation_;

    // Serialises calls into the synchronous fan-out (`fill`). The
    // coordinator thread acquires it for the duration of each item it
    // processes, and direct callers of `fill` also acquire it. Without
    // this, concurrent sync + async fills would race on `current_image_`
    // and `work_generation_`.
    std::mutex fill_mutex_;

    // Async producer/consumer pipeline. SPSC queue feeds the coordinator
    // thread, which calls the synchronous `fill` one image at a time.
    std::unique_ptr<AsyncQueue> async_queue_;
    std::thread coordinator_;
    std::atomic<bool> stop_coordinator_{false};
    std::atomic<bool> coordinator_busy_{false};
    std::chrono::microseconds async_wait_{100};

    // Private worker thread method
    void worker_loop(int thread_id);
    void coordinator_loop();
    int row_start(int thread_id) const;
    int row_count(int thread_id) const;

  public:
    PixelHistogram(int rows, int cols, int n_bins, double xmin, double xmax,
                   int n_threads = 1, std::size_t max_pending = 16);
    ~PixelHistogram();

    // Synchronous fill: blocks until the image has been merged into the
    // accumulators. Safe to call concurrently with `fill_async` (calls are
    // serialised through `fill_mutex_`).
    void fill(const NDView<AxisType, 2> &image);

    // Asynchronous fill: takes ownership of `image`, enqueues it for the
    // coordinator thread, and returns. Blocks the caller only if the queue
    // is full (single-producer, single-consumer queue with a sleep-poll
    // backpressure loop, matching the convention in ClusterFinderMT).
    void fill_async(NDArray<AxisType, 2> image);

    // Wait for all queued async fills to complete. Cheap when the queue
    // is already drained.
    void flush() const;

    // Number of items either queued or currently being processed.
    std::size_t pending() const;

    // Implicitly flushes pending async fills first so the snapshot is
    // consistent with everything that was submitted up to the call.
    NDArray<StorageType, 3> hdata() const;
    NDArray<AxisType, 1> bin_centers() const;
    NDArray<AxisType, 1> bin_edges() const;
};

} // namespace aare
