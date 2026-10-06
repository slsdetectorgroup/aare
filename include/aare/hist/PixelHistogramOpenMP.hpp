#pragma once
#include "aare/NDArray.hpp"
#include "aare/NDView.hpp"
#include "aare/ProducerConsumerQueue.hpp"
#include "aare/hist/PixelHistogramImpl.hpp"
#include <omp.h>

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
class PixelHistogramOpenMP {
  private:
    using Hist = PixelHistogramImpl<AxisType, StorageType>;
    using AsyncQueue = ProducerConsumerQueue<NDArray<AxisType, 2>>;

    int rows_;
    int cols_;
    int n_threads_;
    const AxisType xmin_;
    const AxisType xmax_;

    // processor thread processing the images
    std::thread process_thread;

    std::atomic<bool> m_done_processing_image = false;

    std::atomic<bool> m_stop_processing = false;

    // Async producer/consumer pipeline. SPSC queue feeds the coordinator
    // thread, which fans each image out to the worker pool one at a time.
    // TODO: batch processing?
    // TODO: FIFO to avoid allocations?
    std::unique_ptr<AsyncQueue> async_queue_;

    std::chrono::microseconds async_wait_{100};

    /** fill the histogram with images in async_queue*/
    void fill_histogram();

    Hist histogram{};

  public:
    PixelHistogramOpenMP(int rows, int cols, int n_bins, AxisType xmin,
                         AxisType xmax, int n_threads = 1,
                         std::size_t max_pending = 16);
    ~PixelHistogramOpenMP();

    // Asynchronous fill: takes ownership of `image`, enqueues it for the
    // coordinator thread, and returns. Blocks the caller only if the queue
    // is full (single-producer, single-consumer queue with a sleep-poll
    // backpressure loop, matching the convention in ClusterFinderMT).
    void fill_async(NDArray<AxisType, 2> &&image);

    // Wait for all queued async fills to complete. Cheap when the queue
    // is already drained.
    void flush() const;

    /**
     * @brief Stop the processing thread and wait for it to finish.
     */
    void stop();

    // Implicitly flushes pending async fills first so the snapshot is
    // consistent with everything that was submitted up to the call.
    NDArray<StorageType, 3> values() const;
    NDArray<AxisType, 1> bin_centers() const;
    NDArray<AxisType, 1> bin_edges() const;
};

template <typename StorageType, typename AxisType>
PixelHistogramOpenMP<StorageType, AxisType>::PixelHistogramOpenMP(
    int rows, int cols, int n_bins, AxisType xmin, AxisType xmax, int n_threads,
    std::size_t max_pending)
    : rows_(rows), cols_(cols), n_threads_(n_threads), xmin_(xmin), xmax_(xmax),
      histogram(rows, cols, n_bins, xmin, xmax) {
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

    n_threads_ = std::min(n_threads_, rows_ * cols_);

    // Async pipeline. The PCQ holds (size - 1) usable slots, so size up by
    // one to honour the requested max_pending.
    async_queue_ = std::make_unique<AsyncQueue>(
        static_cast<std::uint32_t>(max_pending + 1));

    process_thread = std::thread(
        &PixelHistogramOpenMP<StorageType, AxisType>::fill_histogram, this);
}

template <typename StorageType, typename AxisType>
PixelHistogramOpenMP<StorageType, AxisType>::~PixelHistogramOpenMP() {
    // Drain any pending async fills before tearing down the worker pool.
    // The coordinator's loop keeps processing while stop_coordinator_ is
    // true as long as the queue is non-empty (mirrors ClusterFinderMT).

    if (process_thread.joinable()) {
        stop();
        process_thread.join();
    }
}

template <typename StorageType, typename AxisType>
void PixelHistogramOpenMP<StorageType, AxisType>::fill_histogram() {

    omp_set_num_threads(n_threads_);

    NDArray<AxisType, 2> image;

#pragma omp parallel
    while (!m_stop_processing) {

#pragma omp single
        {
            while (!async_queue_->read(image) && !m_stop_processing) {
                std::this_thread::sleep_for(async_wait_);
            }
            m_done_processing_image = m_stop_processing ? true : false;
        }

#pragma omp barrier

        if (m_stop_processing)
            break;

// set OMP_WAIT_POLICY=PASSIVE
#pragma omp for
        for (int row = 0; row < rows_; ++row) {
            for (ssize_t col = 0; col < cols_; ++col) {
                const auto val = image(row, col);
                histogram.fill_unchecked(row, static_cast<int>(col), val);
            }
        }

#pragma omp single
        {
            m_done_processing_image = true;
        }
    }
}

template <typename StorageType, typename AxisType>
NDArray<StorageType, 3>
PixelHistogramOpenMP<StorageType, AxisType>::values() const {
    // Make sure any pending async fills are merged in before we snapshot
    // the partial histograms. Cheap when the queue is already drained.
    flush();

    // TODO: maybe its even better to return a view
    NDArray<StorageType, 3> data(histogram.view());

    return data;
}

template <typename StorageType, typename AxisType>
void PixelHistogramOpenMP<StorageType, AxisType>::fill_async(
    NDArray<AxisType, 2> &&image) {

    if (image.shape(0) != rows_ || image.shape(1) != cols_) {
        throw std::invalid_argument("PixelHistogram image shape does "
                                    "not match constructor shape");
    }

    // SPSC backpressure: spin with a short sleep until a slot frees up.
    // The std::move only consumes `image` on the iteration that
    // succeeds (placement-new inside write() runs only when the slot is
    // free).
    while (!async_queue_->write(std::move(image))) {
        std::this_thread::sleep_for(async_wait_);
    }
}

template <typename StorageType, typename AxisType>
void PixelHistogramOpenMP<StorageType, AxisType>::flush() const {
    while (!async_queue_->isEmpty() || !m_done_processing_image) {
        std::this_thread::sleep_for(async_wait_);
    }
}

template <typename StorageType, typename AxisType>
void PixelHistogramOpenMP<StorageType, AxisType>::stop() {
    flush();
    m_stop_processing = true;
}

template <typename StorageType, typename AxisType>
NDArray<AxisType, 1>
PixelHistogramOpenMP<StorageType, AxisType>::bin_centers() const {
    return histogram.bin_centers();
}

template <typename StorageType, typename AxisType>
NDArray<AxisType, 1>
PixelHistogramOpenMP<StorageType, AxisType>::bin_edges() const {
    return histogram.bin_edges();
}

} // namespace aare
