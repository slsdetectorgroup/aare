#pragma once
#include "aare/NDArray.hpp"
#include "aare/NDView.hpp"
#include "aare/ProducerConsumerQueue.hpp"
#include "aare/hist/PixelHistogramImpl.hpp"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <thread>
#include <utility>
#include <vector>

namespace aare {
template <typename StorageType = uint16_t, typename AxisType = float>
class PixelHistogram {
  private:
    using Hist = PixelHistogramImpl<AxisType, StorageType>;
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
    const NDView<AxisType, 2> *current_image_;
    int completed_threads_;
    std::atomic<bool> stop_workers_;
    int work_generation_;

    // Async producer/consumer pipeline. SPSC queue feeds the coordinator
    // thread, which fans each image out to the worker pool one at a time.
    // TODO: batch processing?
    // TODO: FIFO to avoid allocations?
    std::unique_ptr<AsyncQueue> async_queue_;
    std::thread coordinator_;
    std::atomic<bool> stop_coordinator_{false};
    std::atomic<bool> coordinator_busy_{false};
    std::chrono::microseconds async_wait_{100};

    // Private worker thread method
    void worker_loop(int thread_id);
    void coordinator_loop();
    // Fan a single image out to the worker pool and block until every
    // worker has merged its row band. Only ever called by the coordinator
    // thread, so no caller-serialisation lock is needed.
    void dispatch(const NDView<AxisType, 2> &image);
    int row_start(int thread_id) const;
    int row_count(int thread_id) const;

  public:
    PixelHistogram(int rows, int cols, int n_bins, AxisType xmin, AxisType xmax,
                   int n_threads = 1, std::size_t max_pending = 16);
    ~PixelHistogram();

    // Asynchronous fill: takes ownership of `image`, enqueues it for the
    // coordinator thread, and returns. Blocks the caller only if the queue
    // is full (single-producer, single-consumer queue with a sleep-poll
    // backpressure loop, matching the convention in ClusterFinderMT).
    void fill_async(NDArray<AxisType, 2> &&image);

    // Wait for all queued async fills to complete. Cheap when the queue
    // is already drained.
    void flush() const;

    // Implicitly flushes pending async fills first so the snapshot is
    // consistent with everything that was submitted up to the call.
    NDArray<StorageType, 3> values() const;
    NDArray<AxisType, 1> bin_centers() const;
    NDArray<AxisType, 1> bin_edges() const;
};

template <typename StorageType, typename AxisType>
PixelHistogram<StorageType, AxisType>::PixelHistogram(int rows, int cols,
                                                      int n_bins, AxisType xmin,
                                                      AxisType xmax,
                                                      int n_threads,
                                                      std::size_t max_pending)
    : rows_(rows), cols_(cols), n_threads_(n_threads), xmin_(xmin), xmax_(xmax),
      current_image_(nullptr), completed_threads_(0), stop_workers_(false),
      work_generation_(0) {
    if (rows_ < 1 || cols_ < 1 || n_bins < 1) {
        throw std::invalid_argument(
            "PixelHistogram requires positive rows, cols and bins");
    }
    if (n_threads < 1) {
        throw std::invalid_argument(
            "PixelHistogram requires at least one thread");
    }
    if (max_pending < 1) {
        throw std::invalid_argument("PixelHistogram requires max_pending >= 1");
    }

    n_threads_ = std::min(n_threads_, rows_);

    // Build a balanced row partition. With base = rows_ / n_threads_ and
    // extra = rows_ % n_threads_, the first `extra` threads get base + 1
    // rows each and the rest get `base` rows. This avoids the
    // ceil(rows/n_threads) scheme leaving trailing threads with zero or
    // negative row counts (e.g. rows=17, n_threads=8).
    row_offsets_.resize(n_threads_ + 1);
    const int base = rows_ / n_threads_;
    const int extra = rows_ % n_threads_;
    int offset = 0;
    for (int i = 0; i < n_threads_; ++i) {
        row_offsets_[i] = offset;
        offset += base + (i < extra ? 1 : 0);
    }
    row_offsets_[n_threads_] = offset; // == rows_ by construction

    // Initialize partial histograms for each thread
    partial_hists_.reserve(n_threads_);
    for (int i = 0; i < n_threads_; ++i) {
        const auto local_rows = row_count(i);
        partial_hists_.emplace_back(local_rows, cols, n_bins, xmin, xmax);
    }

    // Spawn worker threads
    for (int i = 0; i < n_threads_; ++i) {
        workers_.emplace_back([this, i]() { this->worker_loop(i); });
    }

    // Async pipeline. The PCQ holds (size - 1) usable slots, so size up by
    // one to honour the requested max_pending.
    async_queue_ = std::make_unique<AsyncQueue>(
        static_cast<std::uint32_t>(max_pending + 1));
    coordinator_ = std::thread([this]() { this->coordinator_loop(); });
}

template <typename StorageType, typename AxisType>
PixelHistogram<StorageType, AxisType>::~PixelHistogram() {
    // Drain any pending async fills before tearing down the worker pool.
    // The coordinator's loop keeps processing while stop_coordinator_ is
    // true as long as the queue is non-empty (mirrors ClusterFinderMT).
    if (coordinator_.joinable()) {
        stop_coordinator_ = true;
        coordinator_.join();
    }

    // Signal all workers to stop
    {
        std::unique_lock<std::mutex> lock(work_mutex_);
        stop_workers_ = true;
    }
    work_cv_.notify_all();

    // Join all worker threads
    for (auto &thread : workers_) {
        if (thread.joinable()) {
            thread.join();
        }
    }
}

template <typename StorageType, typename AxisType>
int PixelHistogram<StorageType, AxisType>::row_start(int thread_id) const {
    return row_offsets_[thread_id];
}

template <typename StorageType, typename AxisType>
int PixelHistogram<StorageType, AxisType>::row_count(int thread_id) const {
    return row_offsets_[thread_id + 1] - row_offsets_[thread_id];
}

template <typename StorageType, typename AxisType>
void PixelHistogram<StorageType, AxisType>::worker_loop(int thread_id) {
    int last_generation = 0;

    while (true) {
        std::unique_lock<std::mutex> lock(work_mutex_);
        work_cv_.wait(lock, [this, last_generation]() {
            return work_generation_ != last_generation || stop_workers_;
        });

        if (stop_workers_) {
            break;
        }

        // Get work assignment
        const NDView<AxisType, 2> &image = *current_image_;
        const int generation = work_generation_;
        const int first_row = row_start(thread_id);
        const int local_rows = row_count(thread_id);

        lock.unlock();

        // Do the work: fill this thread's partial histogram. The
        // [xmin, xmax) range gate lives inside PixelHistogramImpl::fill.
        auto &my_hist = partial_hists_[thread_id];
        const auto cols = image.shape(1);
        for (int local_row = 0; local_row < local_rows; ++local_row) {
            const auto row = static_cast<ssize_t>(first_row + local_row);
            for (ssize_t col = 0; col < cols; ++col) {
                const auto val = image(row, col);
                my_hist.fill_unchecked(local_row, static_cast<int>(col), val);
            }
        }

        // Signal completion
        {
            std::unique_lock<std::mutex> done_lock(work_mutex_);
            last_generation = generation;
            completed_threads_++;
            if (completed_threads_ == n_threads_) {
                done_cv_.notify_one();
            }
        }
    }
}

template <typename StorageType, typename AxisType>
NDArray<StorageType, 3> PixelHistogram<StorageType, AxisType>::values() const {
    // Make sure any pending async fills are merged in before we snapshot
    // the partial histograms. Cheap when the queue is already drained.
    flush();

    const auto first_shard_view = partial_hists_.front().view();
    const auto cols = static_cast<ssize_t>(first_shard_view.shape(1));
    const auto bins = static_cast<ssize_t>(first_shard_view.shape(2));
    const auto rows = static_cast<ssize_t>(rows_);

    NDArray<StorageType, 3> data({rows, cols, bins});

    // Each thread owns a disjoint, contiguous range of rows. The shard's
    // dense storage layout [local_row][col][bin] is identical to the slice
    // [first_row .. first_row + local_rows)[col][bin] of `data`, so the
    // merge is just one bulk copy per thread; no per-element accumulation
    // and no upfront zeroing of `data` is needed.
    const size_t pixel_stride = static_cast<size_t>(cols) * bins;
    for (int t = 0; t < n_threads_; ++t) {
        const auto first_row = static_cast<size_t>(row_start(t));
        const auto local_rows = static_cast<size_t>(row_count(t));
        if (local_rows == 0)
            continue;

        const auto shard_view = partial_hists_[t].view();
        std::memcpy(data.data() + first_row * pixel_stride, shard_view.data(),
                    local_rows * pixel_stride * sizeof(StorageType));
    }

    return data;
}

template <typename StorageType, typename AxisType>
void PixelHistogram<StorageType, AxisType>::dispatch(
    const NDView<AxisType, 2> &image) {
    // Called only by the coordinator thread on images already shape-checked
    // by fill_async, so there is no need to re-validate or to serialise
    // against other callers.

    // Reset counters and set work
    {
        std::unique_lock<std::mutex> lock(work_mutex_);
        completed_threads_ = 0;
        current_image_ = &image;
        ++work_generation_;
    }

    // Signal all worker threads to start
    work_cv_.notify_all();

    // Wait for all workers to complete
    {
        std::unique_lock<std::mutex> lock(work_mutex_);
        done_cv_.wait(lock,
                      [this]() { return completed_threads_ == n_threads_; });
        current_image_ = nullptr; // Clear work assignment
    }
}

template <typename StorageType, typename AxisType>
void PixelHistogram<StorageType, AxisType>::fill_async(
    NDArray<AxisType, 2> &&image) {
    if (image.shape(0) != rows_ || image.shape(1) != cols_) {
        throw std::invalid_argument(
            "PixelHistogram image shape does not match constructor shape");
    }

    // SPSC backpressure: spin with a short sleep until a slot frees up.
    // The std::move only consumes `image` on the iteration that succeeds
    // (placement-new inside write() runs only when the slot is free).
    while (!async_queue_->write(std::move(image))) {
        std::this_thread::sleep_for(async_wait_);
    }
}

template <typename StorageType, typename AxisType>
void PixelHistogram<StorageType, AxisType>::flush() const {
    while (!async_queue_->isEmpty() ||
           coordinator_busy_.load(std::memory_order_acquire)) {
        std::this_thread::sleep_for(async_wait_);
    }
}

template <typename StorageType, typename AxisType>
void PixelHistogram<StorageType, AxisType>::coordinator_loop() {
    NDArray<AxisType, 2> item;
    while (!stop_coordinator_.load(std::memory_order_acquire) ||
           !async_queue_->isEmpty()) {
        if (async_queue_->read(item)) {
            coordinator_busy_.store(true, std::memory_order_release);
            dispatch(item.view());
            coordinator_busy_.store(false, std::memory_order_release);
        } else {
            std::this_thread::sleep_for(async_wait_);
        }
    }
}

template <typename StorageType, typename AxisType>
NDArray<AxisType, 1>
PixelHistogram<StorageType, AxisType>::bin_centers() const {
    // All shards share the same value-axis configuration, so any one will
    // do; pick the first.
    return partial_hists_.front().bin_centers();
}

template <typename StorageType, typename AxisType>
NDArray<AxisType, 1> PixelHistogram<StorageType, AxisType>::bin_edges() const {
    return partial_hists_.front().bin_edges();
}

} // namespace aare
