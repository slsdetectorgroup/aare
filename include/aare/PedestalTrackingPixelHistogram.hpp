#pragma once
#include "aare/NDArray.hpp"
#include "aare/NDView.hpp"
#include "aare/Pedestal.hpp"
#include "aare/ProducerConsumerQueue.hpp"

// Lets see if we need to hide it behind a pimpl
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


class PedestalTrackingPixelHistogram {
  public:
    using StorageType = uint16_t;
    using AxisType = float; //TODO: template on pedesta type if needed
    using FrameType = uint16_t;

  private:
    using Axes = std::tuple<
        bh::axis::regular<AxisType, bh::use_default, bh::use_default,
                          bh::axis::option::none_t>,
        bh::axis::integer<int, bh::use_default, bh::axis::option::none_t>,
        bh::axis::integer<int, bh::use_default, bh::axis::option::none_t>>;
    using Hist = bh::histogram<Axes, bh::dense_storage<StorageType>>;
    using AsyncQueue = ProducerConsumerQueue<NDArray<FrameType, 2>>;

    // What kind of fan-out work the worker pool should currently do.
    // Set under work_mutex_; read by worker_loop after wakeup.
    enum class WorkKind { Fill, PushPedestal, UpdateMean, FillWithThreshold };

    int rows_;
    int cols_;
    int n_threads_;
    const AxisType xmin_;
    const AxisType xmax_;

    std::vector<int> row_offsets_;
    std::vector<Hist> partial_hists_;
    // Per-thread pedestal sized [local_rows x cols]. Indexed by the
    // worker using the LOCAL row index (i.e. 0..row_count(t)-1), NOT the
    // global row index. Owned exclusively by worker `t` during a
    // dispatched fan-out.
    std::vector<Pedestal<AxisType>> partial_pedestals_;
    std::vector<NDArray<double, 2>> partial_std_; //cached for pedestal tracking

    // Thread pool members
    std::vector<std::thread> workers_;
    std::mutex work_mutex_;
    std::condition_variable work_cv_;
    std::condition_variable done_cv_;
    WorkKind current_work_kind_;
    const NDView<FrameType, 2> *current_image_;
    std::atomic<int> completed_threads_;
    std::atomic<bool> stop_workers_;
    int work_generation_;

    // Serialises all fan-outs: `fill`, `push_pedestal_no_update`,
    // `update_mean`, and the async coordinator's calls into
    // `fill_with_threshold_`. Always the outermost lock; work_mutex_
    // is taken briefly inside it.
    mutable std::mutex fill_mutex_;

    // Async producer/consumer pipeline. SPSC queue feeds the coordinator
    // thread, which calls `fill_with_threshold_` one image at a time.
    std::unique_ptr<AsyncQueue> async_queue_;
    std::thread coordinator_;
    std::atomic<bool> stop_coordinator_{false};
    std::atomic<bool> coordinator_busy_{false};
    std::chrono::microseconds async_wait_{100};
    std::atomic<double> n_sigma_;

    // Private worker thread method
    void worker_loop(int thread_id);
    void coordinator_loop();
    int row_start(int thread_id) const;
    int row_count(int thread_id) const;

    // Sets up the work generation, wakes workers, and waits for them.
    // Caller MUST already hold `fill_mutex_`. `image` may be nullptr
    // for work kinds that don't need it (e.g. UpdateMean).
    void dispatch_(WorkKind kind, const NDView<FrameType, 2> *image);

    // Coordinator-facing entry point: takes fill_mutex_ and dispatches
    // FillWithThreshold to the worker pool. Same shape as fill().
    void fill_with_threshold_(const NDView<FrameType, 2> &image);

  public:
    PedestalTrackingPixelHistogram(int rows, int cols, int n_bins,
                                   AxisType xmin, AxisType xmax,
                                   int n_threads = 1,
                                   std::size_t max_pending = 16,
                                   double n_sigma = 1.0);
    ~PedestalTrackingPixelHistogram();


    void push_pedestal_no_update(const NDView<FrameType, 2> &frame);
    void update_mean();
    NDArray<AxisType, 2> pedestal_mean() const;

    // Synchronous fill: blocks until the pedestal-subtracted residual
    // for `image` has been merged into the accumulators. Safe to call
    // concurrently with `fill_async` and the
    // pedestal-update API (calls are serialised through `fill_mutex_`).
    // Histogram-only - independent of `n_sigma()`.
    void fill(const NDView<FrameType, 2> &image);

    void fill_async(NDArray<FrameType, 2> image);

    // Sigma multiplier for the pedestal-update gate in
    // fill_async. Atomic; safe to read/write at any
    // time (the new value takes effect on subsequent pixel evaluations).
    double n_sigma() const;
    void set_n_sigma(double n_sigma);

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
