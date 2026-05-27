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
// PedestalTrackingPixelHistogram histograms `frame - pedestal_mean` per
// pixel. Both the pedestal and the histogram are sharded by row range:
// thread t exclusively owns the slice of rows
// [row_offsets_[t], row_offsets_[t + 1]) of BOTH `partial_pedestals_[t]`
// and `partial_hists_[t]`, so no two threads ever touch the same memory
// while a fan-out is in flight.
//
// All four pedestal/histogram-mutating operations (`fill`,
// `push_pedestal_no_update`, `update_mean`, FillWithThreshold) are
// dispatched through the same worker pool via a `WorkKind` switch in
// `worker_loop`. They are serialised against each other by
// `fill_mutex_`, which also serialises the async coordinator's calls
// into `fill_with_threshold_`.
//
// The single async entry point is `fill_async_with_threshold`, which
// histograms the residual AND additionally pushes raw pixel samples
// whose residual is within `n_sigma_ * cached_std` of zero back into
// the per-thread pedestal shard (sigma-clipped pedestal tracking).
// Setting `n_sigma_ = 0.0` disables that pedestal-update side effect,
// recovering plain histogram-only async filling.
//
// Typical usage:
//
//     for (auto& f : pedestal_frames) pth.push_pedestal_no_update(f);
//     pth.update_mean();
//     for (auto& f : measurement_frames)
//         pth.fill_async_with_threshold(std::move(f));
//     pth.flush();
//     auto data = pth.hdata();
class PedestalTrackingPixelHistogram {
  public:
    using StorageType = uint16_t;
    using AxisType = float;
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
    // Cumulative row offsets so that thread t owns rows
    //     [row_offsets_[t], row_offsets_[t + 1])
    // Length is n_threads_ + 1; partition is balanced (the first
    // rows_ % n_threads_ threads get one extra row each).
    std::vector<int> row_offsets_;
    // Per-thread histograms over (residual, col, local_row).
    std::vector<Hist> partial_hists_;
    // Per-thread pedestal sized [local_rows x cols]. Indexed by the
    // worker using the LOCAL row index (i.e. 0..row_count(t)-1), NOT the
    // global row index. Owned exclusively by worker `t` during a
    // dispatched fan-out.
    std::vector<Pedestal<AxisType>> partial_pedestals_;
    // Per-thread cached std, sized [local_rows x cols]. Written by worker
    // thread t inside the UpdateMean case of worker_loop (after the
    // shard's m_mean has been refreshed); read by worker t in the
    // FillWithThreshold case. Same shard-locality contract as
    // partial_pedestals_.
    std::vector<NDArray<double, 2>> partial_std_;

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

    // Sigma multiplier used as the pedestal-update gate in
    // FillWithThreshold. Atomic so the setter can update it without
    // taking fill_mutex_; workers do relaxed loads on each pixel.
    // Setting it to 0.0 disables the pedestal update entirely.
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

    // Accumulate `frame` into the running pedestal estimate without
    // refreshing the cached mean (the cheap path used while
    // bootstrapping the pedestal). Workers update their own shard.
    // Call `update_mean()` once you're done before starting to
    // `fill`/`fill_async_with_threshold`.
    void push_pedestal_no_update(const NDView<FrameType, 2> &frame);

    // Refresh each partial pedestal's cached per-pixel mean from its
    // running sums. Serialises with all other fan-outs through
    // `fill_mutex_` so worker reads of the pedestal mean cannot race.
    void update_mean();

    // Snapshot of the per-pixel pedestal mean, stitched together from
    // all shards into a single `[rows x cols]` array. Implicitly
    // drains pending async fills and takes `fill_mutex_` so it cannot
    // tear against an in-flight `update_mean()` (which is the only
    // operation that overwrites `m_mean`).
    NDArray<AxisType, 2> pedestal_mean() const;

    // Synchronous fill: blocks until the pedestal-subtracted residual
    // for `image` has been merged into the accumulators. Safe to call
    // concurrently with `fill_async_with_threshold` and the
    // pedestal-update API (calls are serialised through `fill_mutex_`).
    // Histogram-only - independent of `n_sigma()`.
    void fill(const NDView<FrameType, 2> &image);

    // Asynchronous fill with sigma-clipped pedestal tracking. Takes
    // ownership of `image`, enqueues it for the coordinator thread, and
    // returns. The worker pool histograms each in-range residual AND
    // additionally pushes the raw pixel value into the pedestal shard
    // when `abs(residual) < n_sigma() * cached_std` (per-pixel gate).
    // `n_sigma() = 0.0` disables the pedestal update, recovering plain
    // histogram-only async filling. Blocks the caller only if the
    // queue is full (single-producer, single-consumer queue with a
    // sleep-poll backpressure loop, matching the convention in
    // ClusterFinderMT).
    void fill_async_with_threshold(NDArray<FrameType, 2> image);

    // Sigma multiplier for the pedestal-update gate in
    // fill_async_with_threshold. Atomic; safe to read/write at any
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
