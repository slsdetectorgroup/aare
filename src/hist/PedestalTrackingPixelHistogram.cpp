#include "aare/hist/PedestalTrackingPixelHistogram.hpp"
#include "aare/File.hpp"
#include "aare/MultiThreadedFileReader.hpp"

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstring>
#include <functional>
#include <future>
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
      current_images_(nullptr), completed_threads_(0), stop_workers_(false),
      work_generation_(0), max_batch_size_(max_pending / 3), n_sigma_(n_sigma) {
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
        partial_hists_.emplace_back(local_rows, cols, n_bins, xmin_, xmax_);
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
        current_images_ = nullptr;
        ++work_generation_;
    }
    work_cv_.notify_all();
    {
        std::unique_lock<std::mutex> lock(work_mutex_);
        done_cv_.wait(lock,
                      [this]() { return completed_threads_ == n_threads_; });
        current_image_ = nullptr;
        current_images_ = nullptr;
    }
}

void PedestalTrackingPixelHistogram::dispatch_fill_batch_(
    const std::vector<NDView<FrameType, 2>> &images) {
    // Caller must already hold fill_mutex_. `images` is owned by
    // fill_with_threshold_batch_ and stays alive until this blocking dispatch
    // returns.
    {
        std::unique_lock<std::mutex> lock(work_mutex_);
        completed_threads_ = 0;
        current_work_kind_ = WorkKind::FillWithThreshold;
        current_image_ = nullptr;
        current_images_ = &images;
        ++work_generation_;
    }
    work_cv_.notify_all();
    {
        std::unique_lock<std::mutex> lock(work_mutex_);
        done_cv_.wait(lock,
                      [this]() { return completed_threads_ == n_threads_; });
        current_images_ = nullptr;
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
        const std::vector<NDView<FrameType, 2>> *images = current_images_;
        const int generation = work_generation_;
        const int first_row = row_start(thread_id);
        const int local_rows = row_count(thread_id);

        lock.unlock();

        auto &my_pedestal = partial_pedestals_[thread_id];
        auto &my_hist = partial_hists_[thread_id];

        switch (kind) {
        case WorkKind::PushPedestal: {
            auto frame = image->sub_view(first_row, first_row + local_rows);
            my_pedestal.push_init(frame);
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
            // back into the pedestal shard. With n_sigma <= 0, use a
            // histogram-only hot path that skips the per-pixel pedestal
            // tracking gate entirely. The [xmin, xmax) histogram gate
            // lives inside PixelHistogramImpl::fill.
            const auto n_sigma = n_sigma_.load(std::memory_order_relaxed);
            constexpr std::size_t pixel_tile_size = 512;
            const auto local_pixels = static_cast<std::size_t>(local_rows) *
                                      static_cast<std::size_t>(cols_);
            const auto global_pixel_begin =
                static_cast<std::size_t>(first_row) *
                static_cast<std::size_t>(cols_);
            const auto frame_count = images->size();

            if (n_sigma <= AxisType{0.0}) {
                // Fill without pedestal tracking. Each worker retains
                // exclusive ownership of its row shard. Pixel tiling keeps a
                // bounded part of that shard's [pixel x bin] storage hot while
                // every input access remains contiguous within one frame.
                for (std::size_t p0 = 0; p0 < local_pixels;
                     p0 += pixel_tile_size) {
                    const auto p1 =
                        std::min(p0 + pixel_tile_size, local_pixels);
                    for (std::size_t f = 0; f < frame_count; ++f) {
                        const auto *input =
                            (*images)[f].data() + global_pixel_begin + p0;
                        for (std::size_t local_pixel = p0; local_pixel < p1;
                             ++local_pixel) {
                            const FrameType raw = input[local_pixel - p0];
                            const AxisType val =
                                static_cast<AxisType>(raw) -
                                static_cast<AxisType>(my_pedestal.mean(
                                    static_cast<ssize_t>(local_pixel)));
                            my_hist.fill_flat_unchecked(local_pixel, val);
                        }
                    }
                }
                break;
            } else {
                // Tracking uses the same tiles, but frames remain strictly
                // chronological for every pixel. push_fast updates that
                // pixel's sums and cached mean immediately. This is equivalent
                // to the old push_no_update followed by a whole-shard
                // update_mean after each frame, since pixels are independent.
                auto &my_std = partial_std_[thread_id];
                for (std::size_t p0 = 0; p0 < local_pixels;
                     p0 += pixel_tile_size) {
                    const auto p1 =
                        std::min(p0 + pixel_tile_size, local_pixels);
                    for (std::size_t f = 0; f < frame_count; ++f) {
                        const auto *input =
                            (*images)[f].data() + global_pixel_begin + p0;
                        for (std::size_t local_pixel = p0; local_pixel < p1;
                             ++local_pixel) {
                            const FrameType raw = input[local_pixel - p0];
                            const AxisType val =
                                static_cast<AxisType>(raw) -
                                my_pedestal.mean(
                                    static_cast<ssize_t>(local_pixel));
                            my_hist.fill_flat_unchecked(local_pixel, val);
                            const AxisType sigma =
                                my_std[static_cast<ssize_t>(local_pixel)];
                            if (sigma > AxisType{0.0} &&
                                std::abs(val) < n_sigma * sigma) {
                                my_pedestal.push_fast<FrameType>(local_pixel,
                                                                 raw);
                            }
                        }
                    }
                }
                break;
            }
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

void PedestalTrackingPixelHistogram::fill_with_threshold_batch_(
    std::vector<NDArray<FrameType, 2>> &batch) {
    // Called only by the coordinator thread on images already shape-checked
    // by fill_async, so there is no need to re-validate. fill_mutex_ is
    // still required: push_pedestal_no_update / update_mean / pedestal_mean
    // can run concurrently and must not race this fan-out.
    std::vector<NDView<FrameType, 2>> views;
    views.reserve(batch.size());
    for (auto &frame : batch) {
        views.push_back(frame.view());
    }

    std::lock_guard<std::mutex> fill_lock(fill_mutex_);
    dispatch_fill_batch_(views);
}

void PedestalTrackingPixelHistogram::fill_async(NDArray<FrameType, 2> &&image) {
    if (image.shape(0) != rows_ || image.shape(1) != cols_) {
        throw std::invalid_argument(
            "PedestalTrackingPixelHistogram image shape does not match "
            "constructor shape");
    }

    // ProducerConsumerQueue is SPSC. Serialising this short producer-side
    // operation also prevents fill_async from racing fill_from_file's direct
    // batch dispatch.
    std::lock_guard<std::mutex> ingestion_lock(ingestion_mutex_);

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
    std::vector<NDArray<FrameType, 2>> batch;
    batch.reserve(max_batch_size_);

    while (!stop_coordinator_.load(std::memory_order_acquire) ||
           !async_queue_->isEmpty()) {
        batch.clear();

        auto *first_item = async_queue_->frontPtr();
        if (first_item == nullptr) {
            std::this_thread::sleep_for(async_wait_);
            continue;
        }

        coordinator_busy_.store(true, std::memory_order_release);
        batch.push_back(std::move(*first_item));
        async_queue_->popFront();

        while (batch.size() < max_batch_size_) {
            auto *item = async_queue_->frontPtr();
            if (item == nullptr) {
                break;
            }
            batch.push_back(std::move(*item));
            async_queue_->popFront();
        }

        fill_with_threshold_batch_(batch);
        completed_async_fills_.fetch_add(batch.size(),
                                         std::memory_order_release);
        coordinator_busy_.store(false, std::memory_order_release);
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
    const std::filesystem::path &fname, ssize_t max_frames, bool verbose,
    std::size_t reader_threads, std::size_t reader_chunk_size) {
    constexpr std::size_t progress_interval = 66;
    auto last = std::chrono::steady_clock::now();
    std::size_t last_reported = 0;

    if (max_frames < -1) {
        throw std::invalid_argument(
            "PedestalTrackingPixelHistogram max_frames must be -1 or "
            "non-negative");
    }

    // Preserve the old max_frames behaviour: values beyond EOF are clamped
    // rather than rejected by MultiThreadedFileReader.
    const auto source_total_frames = File(fname).total_frames();
    const auto n_frames = max_frames == -1
                              ? source_total_frames
                              : std::min(static_cast<std::size_t>(max_frames),
                                         source_total_frames);

    experimental::MultiThreadedFileReader reader(fname, reader_threads,
                                                 reader_chunk_size, n_frames);
    if (reader.rows() != static_cast<size_t>(rows_) ||
        reader.cols() != static_cast<size_t>(cols_)) {
        throw std::invalid_argument("PedestalTrackingPixelHistogram: Frame in "
                                    "file {} has shape ({}, {}) does not match "
                                    "constructor shape");
    }
    if (reader.dtype() != Dtype::UINT16 ||
        reader.bytes_per_frame() != static_cast<std::size_t>(rows_) *
                                        static_cast<std::size_t>(cols_) *
                                        sizeof(FrameType)) {
        throw std::invalid_argument(
            "PedestalTrackingPixelHistogram requires uint16 file frames");
    }

    const auto print_progress = [&](std::size_t done) {
        const auto now = std::chrono::steady_clock::now();
        const double dt = std::chrono::duration<double>(now - last).count();
        const auto done_in_interval = done - last_reported;
        const double fps =
            dt > 0.0 ? static_cast<double>(done_in_interval) / dt : 0.0;

        fmt::print("\rProgress: {}/{} ({:.1f}%)  {:.1f} FPS    ", done,
                   n_frames,
                   n_frames == 0 ? 100.0
                                 : 100.0 * static_cast<double>(done) /
                                       static_cast<double>(n_frames),
                   fps);
        std::fflush(stdout);
        last = now;
        last_reported = done;
    };

    using ReadBuffer = NDArray<FrameType, 3>;
    const auto read_into_buffer = [&reader](ReadBuffer &buffer) {
        const auto frame_count = reader.next_read_frames();
        const auto frames_read = reader.read_into(buffer.buffer());
        if (frames_read != frame_count) {
            throw std::runtime_error(
                "MultiThreadedFileReader returned an incomplete batch");
        }
        return frames_read;
    };

    const auto fill_batch = [this](ReadBuffer &buffer,
                                   std::size_t frame_count) {
        std::vector<NDView<FrameType, 2>> views;
        views.reserve(frame_count);
        auto batch_view = buffer.view();
        for (std::size_t i = 0; i < frame_count; ++i) {
            views.push_back(batch_view(i));
        }
        std::lock_guard<std::mutex> fill_lock(fill_mutex_);
        dispatch_fill_batch_(views);
    };

    // Exclude other queue producers for the duration. Drain anything already
    // submitted before bypassing the coordinator with direct batch dispatch.
    std::lock_guard<std::mutex> ingestion_lock(ingestion_mutex_);
    flush();

    std::size_t completed = 0;
    if (n_frames != 0) {
        // Allocate the maximum wave size once per buffer. The last read may
        // contain fewer frames; its returned count limits the views passed to
        // the histogram workers, leaving the unused tail untouched.
        const auto buffer_capacity = reader.next_read_frames();
        std::array<ReadBuffer, 2> buffers{
            ReadBuffer({static_cast<ssize_t>(buffer_capacity), rows_, cols_}),
            ReadBuffer({static_cast<ssize_t>(buffer_capacity), rows_, cols_})};

        std::size_t current_index = 0;
        auto current_frames = read_into_buffer(buffers[current_index]);
        while (true) {
            const bool has_next = completed + current_frames < n_frames;
            const std::size_t next_index = current_index ^ std::size_t{1};

            // MultiThreadedFileReader::read_into is blocking. Run the next
            // read on a dedicated prefetch task while the current batch is
            // processed by the histogram worker pool.
            std::future<std::size_t> next;
            if (has_next) {
                next = std::async(std::launch::async, read_into_buffer,
                                  std::ref(buffers[next_index]));
            }

            std::exception_ptr fill_error;
            try {
                fill_batch(buffers[current_index], current_frames);
            } catch (...) {
                fill_error = std::current_exception();
            }

            // Always observe completion of an in-flight read before unwinding;
            // its workers reference both `reader` and the next batch buffer.
            if (fill_error) {
                if (next.valid()) {
                    try {
                        (void)next.get();
                    } catch (...) {
                        // Preserve the earlier histogram exception.
                    }
                }
                std::rethrow_exception(fill_error);
            }
            completed += current_frames;

            if (verbose && (completed - last_reported >= progress_interval ||
                            completed == n_frames)) {
                print_progress(completed);
            }

            if (!has_next) {
                break;
            }
            current_frames = next.get();
            current_index = next_index;
        }
    }

    if (verbose) {
        if (completed > last_reported || n_frames == 0) {
            print_progress(completed);
        }
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
