#include "aare/hist/PedestalTrackingPixelHistogram.hpp"
#include "aare/File.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstring>
#include <stdexcept>
#include <utility>
#include <vector>

#include <fmt/format.h>
namespace aare {

PedestalTrackingPixelHistogram::PedestalTrackingPixelHistogram(
    int rows, int cols, int n_bins, AxisType xmin, AxisType xmax, int n_threads,
    std::size_t max_pending, AxisType n_sigma)
    : rows_(rows), cols_(cols), n_threads_(n_threads), xmin_(xmin), xmax_(xmax),
      current_work_kind_(WorkKind::FillWithThreshold), current_image_(nullptr),
      completed_threads_(0), stop_workers_(false), work_generation_(0),
      n_sigma_(n_sigma) {
    if (rows_ < 1 || cols_ < 1 || n_bins < 1) {
        throw std::invalid_argument("PedestalTrackingPixelHistogram requires "
                                    "positive rows, cols and bins");
    }
    if (n_threads < 1) {
        throw std::invalid_argument(
            "PedestalTrackingPixelHistogram requires at least one thread");
    }
    if (max_pending < 1) {
        throw std::invalid_argument(
            "PedestalTrackingPixelHistogram requires max_pending >= 1");
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

    // Initialize partial histograms, partial pedestals and the cached
    // per-pixel std for each thread. All three are sized to the
    // thread's row slice and indexed by local_row (0..local_rows-1),
    // so the worker can address them with the same coordinates.
    partial_hists_.reserve(n_threads_);
    partial_pedestals_.reserve(n_threads_);
    partial_std_.reserve(n_threads_);
    for (int i = 0; i < n_threads_; ++i) {
        const auto local_rows = row_count(i);
        partial_hists_.emplace_back(local_rows, cols, n_bins, xmin, xmax);
        partial_pedestals_.emplace_back(static_cast<uint32_t>(local_rows),
                                        static_cast<uint32_t>(cols));
        partial_std_.emplace_back(NDArray<AxisType, 2>(
            {static_cast<ssize_t>(local_rows), static_cast<ssize_t>(cols)},
            0.0));
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

PedestalTrackingPixelHistogram::~PedestalTrackingPixelHistogram() {
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

int PedestalTrackingPixelHistogram::row_start(int thread_id) const {
    return row_offsets_[thread_id];
}

int PedestalTrackingPixelHistogram::row_count(int thread_id) const {
    return row_offsets_[thread_id + 1] - row_offsets_[thread_id];
}

void PedestalTrackingPixelHistogram::dispatch_(
    WorkKind kind, const NDView<FrameType, 2> *image) {
    // Caller must already hold fill_mutex_. Reset counters, publish the
    // new work item, wake the workers, wait for completion.
    {
        std::unique_lock<std::mutex> lock(work_mutex_);
        completed_threads_ = 0;
        current_work_kind_ = kind;
        current_image_ = image;
        ++work_generation_;
    }
    work_cv_.notify_all();
    {
        std::unique_lock<std::mutex> lock(work_mutex_);
        done_cv_.wait(lock,
                      [this]() { return completed_threads_ == n_threads_; });
        current_image_ = nullptr;
    }
}

void PedestalTrackingPixelHistogram::push_pedestal_no_update(
    const NDView<FrameType, 2> &frame) {
    if (frame.shape(0) != rows_ || frame.shape(1) != cols_) {
        throw std::invalid_argument(
            "PedestalTrackingPixelHistogram frame shape does not match "
            "constructor shape");
    }
    std::lock_guard<std::mutex> fill_lock(fill_mutex_);
    dispatch_(WorkKind::PushPedestal, &frame);
}

void PedestalTrackingPixelHistogram::update_mean() {
    // Drain any in-flight async fills first; the coordinator does NOT
    // hold fill_mutex_ at that point, so we can grab it safely after.
    flush();
    std::lock_guard<std::mutex> fill_lock(fill_mutex_);
    dispatch_(WorkKind::UpdateMean, nullptr);
}

void PedestalTrackingPixelHistogram::worker_loop(int thread_id) {
    int last_generation = 0;

    while (true) {
        std::unique_lock<std::mutex> lock(work_mutex_);
        work_cv_.wait(lock, [this, last_generation]() {
            return work_generation_ != last_generation || stop_workers_;
        });

        if (stop_workers_) {
            break;
        }

        // Snapshot the work assignment under the lock so we don't race
        // against the next dispatch publishing new state.
        const WorkKind kind = current_work_kind_;
        const NDView<FrameType, 2> *image = current_image_;
        const int generation = work_generation_;
        const int first_row = row_start(thread_id);
        const int local_rows = row_count(thread_id);

        lock.unlock();

        auto &my_pedestal = partial_pedestals_[thread_id];
        auto &my_hist = partial_hists_[thread_id];

        switch (kind) {
        case WorkKind::PushPedestal: {
            // Accumulate raw frame values into this thread's pedestal
            // shard. Uses the pixel-level push_no_update which only
            // touches m_sum/m_sum2/m_cur_samples (no m_mean writes).
            for (int local_row = 0; local_row < local_rows; ++local_row) {
                const auto row = static_cast<ssize_t>(first_row + local_row);
                for (ssize_t col = 0; col < image->shape(1); ++col) {
                    my_pedestal.template push_no_update<FrameType>(
                        static_cast<uint32_t>(local_row),
                        static_cast<uint32_t>(col), (*image)(row, col));
                }
            }
            break;
        }
        case WorkKind::UpdateMean: {
            // Recompute m_mean from the running sums. Only touches this
            // thread's shard. Also refresh the cached per-pixel std so
            // FillWithThreshold can read it without recomputing on the
            // hot path.
            my_pedestal.update_mean();
            auto &my_std = partial_std_[thread_id];
            for (int local_row = 0; local_row < local_rows; ++local_row) {
                for (int col = 0; col < cols_; ++col) {
                    my_std(local_row, col) = static_cast<AxisType>(
                        my_pedestal.std(static_cast<uint32_t>(local_row),
                                        static_cast<uint32_t>(col)));
                }
            }
            break;
        }
        case WorkKind::FillWithThreshold: {
            // Histogram the pedestal-subtracted residual AND, for pixels
            // whose residual is consistent with noise
            // (|residual| < n_sigma * cached_std), feed the raw value
            // back into the pedestal shard. With n_sigma == 0 the gate
            // never fires, recovering plain histogram-only behaviour
            // (modulo the per-pixel gate evaluation). The [xmin, xmax)
            // histogram gate lives inside PixelHistogramImpl::fill.
            auto &my_std = partial_std_[thread_id];
            const auto n_sigma = n_sigma_.load(std::memory_order_relaxed);
            const auto cols = image->shape(1);
            for (int local_row = 0; local_row < local_rows; ++local_row) {
                const auto row = static_cast<ssize_t>(first_row + local_row);
                for (ssize_t col = 0; col < cols; ++col) {
                    const FrameType raw = (*image)(row, col);
                    const AxisType val = static_cast<AxisType>(raw) -
                                         static_cast<AxisType>(my_pedestal.mean(
                                             static_cast<uint32_t>(local_row),
                                             static_cast<uint32_t>(col)));
                    my_hist.fill_unchecked(local_row, static_cast<int>(col),
                                           val);
                    const AxisType sigma = my_std(local_row, col);
                    if (sigma > 0.0 && std::abs(static_cast<AxisType>(val)) <
                                           n_sigma * sigma) {
                        my_pedestal.template push<FrameType>(
                            static_cast<uint32_t>(local_row),
                            static_cast<uint32_t>(col), raw);
                    }
                }
            }
            break;
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

NDArray<PedestalTrackingPixelHistogram::StorageType, 3>
PedestalTrackingPixelHistogram::values() const {
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

NDArray<PedestalTrackingPixelHistogram::AxisType, 2>
PedestalTrackingPixelHistogram::pedestal_mean() const {
    // Drain in-flight async fills and serialise with all other fan-outs
    // (Fill / PushPedestal / UpdateMean). m_mean is overwritten wholesale
    // by Pedestal::update_mean, so without the lock we could read torn
    // rows mid-update.
    flush();
    std::lock_guard<std::mutex> lock(fill_mutex_);

    NDArray<AxisType, 2> data(
        {static_cast<ssize_t>(rows_), static_cast<ssize_t>(cols_)});

    // Each partial pedestal stores its slice of m_mean in C-order
    // [local_rows x cols], identical in layout to the corresponding
    // [first_row .. first_row + local_rows)[col] slice of `data`, so
    // we can copy each shard with a single memcpy.
    const size_t row_stride = static_cast<size_t>(cols_);
    for (int t = 0; t < n_threads_; ++t) {
        const auto first_row = static_cast<size_t>(row_start(t));
        const auto local_rows = static_cast<size_t>(row_count(t));
        if (local_rows == 0)
            continue;

        const auto view = partial_pedestals_[t].view();
        std::memcpy(data.data() + first_row * row_stride, view.data(),
                    local_rows * row_stride * sizeof(AxisType));
    }

    return data;
}

void PedestalTrackingPixelHistogram::fill_with_threshold_(
    const NDView<FrameType, 2> &image) {
    // Called only by the coordinator thread on images already shape-checked
    // by fill_async, so there is no need to re-validate. fill_mutex_ is
    // still required: push_pedestal_no_update / update_mean / pedestal_mean
    // can run concurrently and must not race this fan-out.
    std::lock_guard<std::mutex> fill_lock(fill_mutex_);
    dispatch_(WorkKind::FillWithThreshold, &image);
}

void PedestalTrackingPixelHistogram::fill_async(NDArray<FrameType, 2> &&image) {
    if (image.shape(0) != rows_ || image.shape(1) != cols_) {
        throw std::invalid_argument(
            "PedestalTrackingPixelHistogram image shape does not match "
            "constructor shape");
    }

    // SPSC backpressure: spin with a short sleep until a slot frees up.
    // The std::move only consumes `image` on the iteration that succeeds
    // (placement-new inside write() runs only when the slot is free).
    while (!async_queue_->write(std::move(image))) {
        std::this_thread::sleep_for(async_wait_);
    }
}

PedestalTrackingPixelHistogram::AxisType
PedestalTrackingPixelHistogram::n_sigma() const {
    return n_sigma_.load(std::memory_order_relaxed);
}

void PedestalTrackingPixelHistogram::set_n_sigma(
    PedestalTrackingPixelHistogram::AxisType n_sigma) {
    n_sigma_.store(n_sigma, std::memory_order_relaxed);
}

void PedestalTrackingPixelHistogram::flush() const {
    while (!async_queue_->isEmpty() ||
           coordinator_busy_.load(std::memory_order_acquire)) {
        std::this_thread::sleep_for(async_wait_);
    }
}

void PedestalTrackingPixelHistogram::coordinator_loop() {
    while (!stop_coordinator_.load(std::memory_order_acquire) ||
           !async_queue_->isEmpty()) {

        auto *item = async_queue_->frontPtr();
        if (item != nullptr) {
            coordinator_busy_.store(true, std::memory_order_release);
            fill_with_threshold_(item->view());
            async_queue_->popFront();
            coordinator_busy_.store(false, std::memory_order_release);
        } else {
            std::this_thread::sleep_for(async_wait_);
        }
    }
}

NDArray<PedestalTrackingPixelHistogram::AxisType, 1>
PedestalTrackingPixelHistogram::bin_centers() const {
    // All shards share the same value-axis configuration, so any one will
    // do; pick the first.
    return partial_hists_.front().bin_centers();
}

NDArray<PedestalTrackingPixelHistogram::AxisType, 1>
PedestalTrackingPixelHistogram::bin_edges() const {
    return partial_hists_.front().bin_edges();
}

void PedestalTrackingPixelHistogram::fill_from_file(
    const std::filesystem::path &fname, ssize_t max_frames, bool verbose) {
    constexpr std::size_t progress_interval = 66;
    auto last = std::chrono::steady_clock::now();

    File f(fname);
    // check that row col matches constructor
    if (f.rows() != static_cast<size_t>(rows_) ||
        f.cols() != static_cast<size_t>(cols_)) {
        throw std::invalid_argument("PedestalTrackingPixelHistogram: Frame in "
                                    "file {} has shape ({}, {}) does not match "
                                    "constructor shape");
    }

    const ssize_t total_frames = f.total_frames();
    const ssize_t n_frames =
        max_frames == -1 ? total_frames : std::min(max_frames, total_frames);
    for (ssize_t i = 0; i < n_frames; ++i) {
        aare::NDArray<uint16_t> frame({rows_, cols_});
        f.read_into(reinterpret_cast<std::byte *>(frame.data()));
        fill_async(std::move(frame));

        // print progress
        if (verbose &&
            ((i + 1) % progress_interval == 0 || (i + 1 == n_frames))) {
            const auto now = std::chrono::steady_clock::now();
            const double dt = std::chrono::duration<double>(now - last).count();
            const std::size_t done_in_interval =
                (i + 1) % progress_interval == 0 ? progress_interval
                                                 : (i + 1) % progress_interval;
            const double fps =
                dt > 0.0 ? static_cast<double>(done_in_interval) / dt : 0.0;
            // Carriage return (no newline) so the line is rewritten
            // in place; flush since stdout is line-buffered.

            fmt::print("\rProgress: {}/{} ({:.1f}%)  {:.1f} FPS    ", i + 1,
                       n_frames,
                       100.0 * static_cast<double>(i + 1) /
                           static_cast<double>(n_frames),
                       fps);
            std::fflush(stdout);
            last = now;
        }
    }
    flush();
    if (verbose) {
        fmt::print("\n\n");
        std::fflush(stdout);
    }
}

void PedestalTrackingPixelHistogram::process_pedestal_file(
    const std::filesystem::path &fname, ssize_t max_frames, bool verbose) {
    constexpr std::size_t progress_interval = 66;
    auto last = std::chrono::steady_clock::now();

    File f(fname);
    // check that row col matches constructor
    if (f.rows() != static_cast<size_t>(rows_) ||
        f.cols() != static_cast<size_t>(cols_)) {
        throw std::invalid_argument("PedestalTrackingPixelHistogram: Frame in "
                                    "file {} has shape ({}, {}) does not match "
                                    "constructor shape");
    }

    const ssize_t total_frames = f.total_frames();
    const ssize_t n_frames =
        max_frames == -1 ? total_frames : std::min(max_frames, total_frames);

    aare::NDArray<uint16_t> frame({rows_, cols_});
    for (ssize_t i = 0; i < n_frames; ++i) {
        f.read_into(reinterpret_cast<std::byte *>(frame.data()));
        push_pedestal_no_update(frame.view());
        if (verbose &&
            ((i + 1) % progress_interval == 0 || (i + 1 == n_frames))) {
            const auto now = std::chrono::steady_clock::now();
            const double dt = std::chrono::duration<double>(now - last).count();
            const std::size_t done_in_interval =
                (i + 1) % progress_interval == 0 ? progress_interval
                                                 : (i + 1) % progress_interval;
            const double fps =
                dt > 0.0 ? static_cast<double>(done_in_interval) / dt : 0.0;
            fmt::print("\rProgress: {}/{} ({:.1f}%)  {:.1f} FPS    ", i + 1,
                       n_frames,
                       100.0 * static_cast<double>(i + 1) /
                           static_cast<double>(n_frames),
                       fps);
            std::fflush(stdout);
            last = now;
        }
    }
    update_mean();
    flush();
    if (verbose) {
        fmt::print("\n\n");
        std::fflush(stdout);
    }
}

} // namespace aare
