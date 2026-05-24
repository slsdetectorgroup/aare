#include "aare/PixelHistogram.hpp"

#include <algorithm>
#include <boost/histogram.hpp>
#include <boost/histogram/storage_adaptor.hpp>
#include <boost/histogram/unsafe_access.hpp>
#include <cstring>
#include <stdexcept>
#include <vector>

namespace aare {


PixelHistogram::PixelHistogram(int rows, int cols, int n_bins, double xmin, double xmax, int n_threads):
             rows_(rows), cols_(cols), n_threads_(n_threads), xmin_(xmin), xmax_(xmax), current_image_(nullptr),
             completed_threads_(0), stop_workers_(false), work_generation_(0) {
    if (rows_ < 1 || cols_ < 1 || n_bins < 1) {
        throw std::invalid_argument("PixelHistogram requires positive rows, cols and bins");
    }
    if (n_threads < 1) {
        throw std::invalid_argument("PixelHistogram requires at least one thread");
    }

    n_threads_ = std::min(n_threads_, rows_);

    // Build a balanced row partition. With base = rows_ / n_threads_ and
    // extra = rows_ % n_threads_, the first `extra` threads get base + 1
    // rows each and the rest get `base` rows. This avoids the
    // ceil(rows/n_threads) scheme leaving trailing threads with zero or
    // negative row counts (e.g. rows=17, n_threads=8).
    row_offsets_.resize(n_threads_ + 1);
    const int base  = rows_ / n_threads_;
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
        partial_hists_.emplace_back(
            bh::axis::regular<AxisType, bh::use_default, bh::use_default,
                              bh::axis::option::none_t>(n_bins, xmin, xmax, "value"),
            bh::axis::integer<int, bh::use_default, bh::axis::option::none_t>(0, cols, "y"),
            bh::axis::integer<int, bh::use_default, bh::axis::option::none_t>(0, local_rows, "x")
        );
    }

    // Spawn worker threads
    for (int i = 0; i < n_threads_; ++i) {
        workers_.emplace_back([this, i]() { this->worker_loop(i); });
    }
}

PixelHistogram::~PixelHistogram() {
    // Signal all workers to stop
    {
        std::unique_lock<std::mutex> lock(work_mutex_);
        stop_workers_ = true;
    }
    work_cv_.notify_all();
    
    // Join all worker threads
    for (auto& thread : workers_) {
        if (thread.joinable()) {
            thread.join();
        }
    }
}

int PixelHistogram::row_start(int thread_id) const {
    return row_offsets_[thread_id];
}

int PixelHistogram::row_count(int thread_id) const {
    return row_offsets_[thread_id + 1] - row_offsets_[thread_id];
}

void PixelHistogram::worker_loop(int thread_id) {
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
        const NDView<AxisType, 2>& image = *current_image_;
        const int generation = work_generation_;
        const int first_row = row_start(thread_id);
        const int local_rows = row_count(thread_id);
        
        lock.unlock();
        
        // Do the work: fill this thread's partial histogram
        for (int local_row = 0; local_row < local_rows; ++local_row) {
            const auto row = static_cast<ssize_t>(first_row + local_row);
            for (ssize_t col = 0; col < image.shape(1); ++col) {
                const auto val = image(row, col);
                if (val < xmin_ || val >= xmax_) {
                    continue; // Skip out-of-range values
                }
                partial_hists_[thread_id](val, col, local_row);
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

NDArray<PixelHistogram::StorageType, 3> PixelHistogram::hdata() const {
    const auto &hist = partial_hists_.front();
    const auto bins = static_cast<ssize_t>(hist.axis(0).size());
    const auto cols = static_cast<ssize_t>(hist.axis(1).size());
    const auto rows = static_cast<ssize_t>(rows_);

    NDArray<StorageType, 3> data({rows, cols, bins});

    // Each thread owns a disjoint, contiguous range of rows and its dense
    // storage layout [local_row][col][bin] is identical to the slice
    // [first_row .. first_row + local_rows)[col][bin] of `data`. So the
    // merge is just one bulk copy per thread; no per-element accumulation
    // and no upfront zeroing of `data` is needed.
    const size_t pixel_stride = static_cast<size_t>(cols) * bins;
    for (int t = 0; t < n_threads_; ++t) {
        const auto &storage = bh::unsafe_access::storage(partial_hists_[t]);
        const StorageType *partial_data = storage.data();
        const auto first_row  = static_cast<size_t>(row_start(t));
        const auto local_rows = static_cast<size_t>(row_count(t));
        if (local_rows == 0) continue;

        std::memcpy(data.data() + first_row * pixel_stride,
                    partial_data,
                    local_rows * pixel_stride * sizeof(StorageType));
    }

    return data;
}

void PixelHistogram::fill(const NDView<AxisType, 2> &image) {
    if (image.shape(0) != rows_ || image.shape(1) != cols_) {
        throw std::invalid_argument("PixelHistogram image shape does not match constructor shape");
    }

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
        done_cv_.wait(lock, [this]() { return completed_threads_ == n_threads_; });
        current_image_ = nullptr;  // Clear work assignment
    }
}

NDArray<PixelHistogram::AxisType, 1> PixelHistogram::bin_centers() const {
    const auto& value_axis = partial_hists_.front().axis(0);
    const auto n_bins = static_cast<ssize_t>(value_axis.size());
    
    NDArray<AxisType, 1> centers({n_bins});
    
    for (ssize_t i = 0; i < n_bins; ++i) {
        // Get the left and right edges of the bin and compute the center
        AxisType left = value_axis.value(i);
        AxisType right = value_axis.value(i + 1);
        centers(i) = (left + right) / AxisType(2.0);
    }
    
    return centers;
}

NDArray<PixelHistogram::AxisType, 1> PixelHistogram::bin_edges() const {
    const auto& value_axis = partial_hists_.front().axis(0);
    const auto n_bins = static_cast<ssize_t>(value_axis.size());

    NDArray<AxisType, 1> edges({n_bins + 1});

    for (ssize_t i = 0; i <= n_bins; ++i) {
        edges(i) = value_axis.value(i);
    }

    return edges;
}

} // namespace aare
