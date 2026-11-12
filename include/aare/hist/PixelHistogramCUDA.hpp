// SPDX-License-Identifier: MPL-2.0
#pragma once
#include "aare/NDArray.hpp"
#include "aare/NDView.hpp"
#include "aare/RAII_device.hpp"
#include "aare/hist/pixelhistogram_kernel.cuh"
#include "aare/utils/cuda_check.cuh"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <iterator>
#include <limits>
#include <stdexcept>
#include <string>
#include <type_traits>

#ifdef AARE_TESTS
#include "../tests/friend_test.hpp"
#endif

namespace aare::cuda {

/// Per-pixel histograms of a stack of frames, filled on the GPU.
///
/// Frames are copied to the GPU in batches of batch_size, as InputType, and
/// histogrammed there into one accumulator of DeviceStorageType counts that
/// stays on the GPU for the whole run. values() caps the counts at what
/// StorageType holds and copies them back, shape (rows, cols, n_bins).
///
/// Counts equal those of PixelHistogram<StorageType, AxisType> on the same
/// frames (AxisType: float for integer frames, the frame type otherwise).
/// Another axis type can bin a value that lies on a bin edge differently.
///
/// The copy of one batch overlaps the kernel on the previous one. With the
/// default window kernel the copy, not the kernel, sets the speed.
///
/// GPU memory: rows * cols * n_bins * sizeof(DeviceStorageType) for the
/// accumulator, 2 * batch_size * rows * cols * sizeof(InputType) for the frame
/// buffers, and during values() a temporary histogram of StorageType, unless
/// StorageType is DeviceStorageType.
///
/// Not thread-safe: one object must not be used from two threads at once.
///
/// @tparam StorageType type of the counts values() returns. Capped at its
///         maximum, like the CPU class.
/// @tparam DeviceStorageType unsigned integer type of the GPU counters. Use
///         uint64_t with 8-byte StorageType so counts can't wrap at 2^32.
/// @tparam InputType type of the frames: floating point, or an integer of at
///         most 16 bits.
/// @tparam BLOCK_SIZE threads per block of the fill kernels.
template <typename StorageType = uint16_t,
          typename DeviceStorageType = uint32_t, typename InputType = float,
          int BLOCK_SIZE = 128>
class PixelHistogramCUDA {
#ifdef AARE_TESTS
    // Writes a known pattern into d_histogram to test values() without the
    // fill kernel (PixelHistogramCUDA_testimpl.cu).
    FRIEND_TEST(known_pattern_cap)
#endif

    // Frames are copied to the device as InputType, as the caller provides
    // them. The histogram axis (range, bin edges, the bin arithmetic) must be
    // floating point: an integral input gets a float axis, converted in the
    // kernel. That conversion is exact up to 16-bit integers.
    static_assert(std::is_floating_point_v<InputType> ||
                      (std::is_integral_v<InputType> && sizeof(InputType) <= 2),
                  "PixelHistogramCUDA: InputType must be floating point or an "
                  "integer of at most 16 bits");
    // The kernels increment the counters with ++ and values() caps them at
    // an integer maximum: a floating-point counter would stop counting
    // exactly, a signed one could overflow.
    static_assert(std::is_integral_v<DeviceStorageType> &&
                      std::is_unsigned_v<DeviceStorageType>,
                  "PixelHistogramCUDA: DeviceStorageType must be an unsigned "
                  "integer");

  public:
    /// Type of the range, the bin edges and the bin arithmetic.
    using AxisType = std::conditional_t<std::is_floating_point_v<InputType>,
                                        InputType, float>;

  private:
    // nrows is declared first: its initializer validates all constructor
    // arguments, so it must run before anything below allocates.
    size_t nrows;
    size_t ncols;
    size_t m_nbins;
    AxisType m_xmin;
    AxisType m_xmax;
    AxisType m_scale; // 1/bin width
    size_t m_batch_size;
    bool m_window;       // fill_window (true) or fill_direct (false)
    size_t m_next_batch; // batches submitted; picks the slot

    static constexpr int NUM_SLOTS =
        2; // Number of concurrent batches to process

    // The order of the declarations below is important. It conditions the order
    // of their destruction.
    HostRegistration m_registered_input{};
    HostRegistration m_registered_output{};

    // Device buffers for frames and histogram
    DeviceBuffer<DeviceStorageType> d_histogram;
    // Per-pixel window centre of fill_window, in bins; NaN until placed.
    DeviceBuffer<float> d_centre;
    std::array<DeviceBuffer<InputType>, NUM_SLOTS> d_frames_batch;

    // fill_window's shared memory per block. Over 48 KB would need
    // cudaFuncSetAttribute(cudaFuncAttributeMaxDynamicSharedMemorySize).
    static constexpr size_t WINDOW_SHMEM_BYTES =
        size_t{detail::WINDOW_BINS} * BLOCK_SIZE * sizeof(uint16_t);
    static_assert(WINDOW_SHMEM_BYTES <= 48 * 1024,
                  "PixelHistogramCUDA: window shared memory exceeds 48 KB per "
                  "block; lower BLOCK_SIZE or WINDOW_BINS");
    // Largest batch whose counts fit fill_window's uint16_t shared counters.
    static constexpr size_t MAX_WINDOW_BATCH = 65535;

    // Events for synchronization
    std::array<Event, NUM_SLOTS> copy_done;
    std::array<Event, NUM_SLOTS> kernel_done;

    // Streams for overlapping copy and compute
    Stream copy_stream;
    Stream compute_stream;

    // Throws on invalid arguments, otherwise returns rows. Called from the
    // first member initializer so that nothing is allocated before the
    // arguments are known to be valid.
    static size_t check_args(size_t rows, size_t cols, size_t n_bins,
                             AxisType xmin, AxisType xmax, size_t batch_size,
                             bool window) {
        if (rows < 1 || cols < 1 || n_bins < 1) {
            throw std::invalid_argument(
                "PixelHistogramCUDA requires positive rows, cols and bins");
        }
        if (!std::isfinite(xmin) || !std::isfinite(xmax)) {
            throw std::invalid_argument(
                "PixelHistogramCUDA requires finite xmin and xmax");
        }
        if (!(xmax > xmin)) {
            throw std::invalid_argument(
                "PixelHistogramCUDA requires xmax > xmin");
        }
        if (batch_size < 1) {
            throw std::invalid_argument(
                "PixelHistogramCUDA requires batch_size >= 1");
        }
        if (window && batch_size > MAX_WINDOW_BATCH) {
            throw std::invalid_argument(
                "PixelHistogramCUDA with window=true requires batch_size <= " +
                std::to_string(MAX_WINDOW_BATCH));
        }
        return rows;
    }

    // Largest count values() can return: the maximum of StorageType, limited
    // to what the device counter can hold. Floating-point storage has no
    // meaningful integer maximum, so it only gets the device limit.
    static constexpr DeviceStorageType count_cap() {
        constexpr auto device_max =
            std::numeric_limits<DeviceStorageType>::max();
        if constexpr (std::is_integral_v<StorageType>) {
            constexpr auto storage_max =
                std::numeric_limits<StorageType>::max();
            if constexpr (static_cast<std::uintmax_t>(storage_max) <
                          static_cast<std::uintmax_t>(device_max)) {
                return static_cast<DeviceStorageType>(storage_max);
            }
        }
        return device_max;
    }

  public:
    /// n_bins equal bins over [xmin, xmax); values outside, and NaN, are not
    /// counted. Throws std::invalid_argument for zero rows, cols, n_bins or
    /// batch_size, a non-finite range or xmax <= xmin.
    ///
    /// @param batch_size frames copied and histogrammed per step. Two batches
    ///        are held on the GPU (see the class description): lower it for
    ///        large frames. With the default 1024 the kernel needs under half
    ///        the copy time, so the copy sets the speed.
    /// @param window true: fill_window counts each pixel's 128 bins around its
    ///        noise peak in shared memory (needs batch_size <= 65535). false:
    ///        fill_direct, every value straight to GPU memory. Both give
    ///        identical counts; window is faster for fine bins.
    PixelHistogramCUDA(size_t rows, size_t cols, size_t n_bins, AxisType xmin,
                       AxisType xmax, size_t batch_size = 1024,
                       bool window = true)
        : nrows{check_args(rows, cols, n_bins, xmin, xmax, batch_size, window)},
          ncols{cols}, m_nbins{n_bins}, m_xmin{xmin}, m_xmax{xmax},
          m_scale{m_nbins / (m_xmax - m_xmin)}, m_batch_size{batch_size},
          m_window{window}, m_next_batch{0},
          d_histogram{nrows * ncols * m_nbins}, d_centre{nrows * ncols},
          copy_stream{make_stream()}, compute_stream{make_stream()} {

        // Construct events
        for (int i = 0; i < NUM_SLOTS; ++i) {
            copy_done[i] = make_event();
            kernel_done[i] = make_event();
        }

        CUDA_CHECK(cudaMemsetAsync(d_histogram.get(), 0, d_histogram.bytes(),
                                   compute_stream.get()));
        // All bytes 0xFF is a float NaN: every window starts "not placed".
        CUDA_CHECK(cudaMemsetAsync(d_centre.get(), 0xFF, d_centre.bytes(),
                                   compute_stream.get()));

        // Initialize device buffers for frames
        for (int i = 0; i < NUM_SLOTS; ++i) {
            d_frames_batch[i] =
                DeviceBuffer<InputType>(nrows * ncols * m_batch_size);
        }
    }

    PixelHistogramCUDA(const PixelHistogramCUDA &) = delete;
    PixelHistogramCUDA &operator=(const PixelHistogramCUDA &) = delete;
    PixelHistogramCUDA(PixelHistogramCUDA &&) = delete;
    PixelHistogramCUDA &operator=(PixelHistogramCUDA &&) = delete;

    /// Pins the caller's frames (page-locks them), so the copies to the GPU
    /// run at full bandwidth and overlap the kernels. Pass the whole dataset
    /// once; slices of it passed to fill_histogram() benefit too. The memory
    /// must outlive the registration. Registering again releases the previous
    /// buffer. read_only: the memory is mapped read-only (e.g. a read-only
    /// memory map), which needs cudaHostRegisterReadOnly. A buffer can be
    /// registered by only one object at a time (a CUDA limit).
    void register_input_buffer(const void *ptr, size_t bytes,
                               bool read_only = false) {
        CUDA_CHECK(cudaStreamSynchronize(
            copy_stream.get())); // Wait for copies to end in the stream
        m_registered_input.unregister();
        m_registered_input = HostRegistration(
            const_cast<void *>(ptr), bytes,
            read_only ? cudaHostRegisterReadOnly : cudaHostRegisterDefault);
    }

    /// Releases the buffer pinned by register_input_buffer().
    void unregister_input_buffer() {
        CUDA_CHECK(cudaStreamSynchronize(copy_stream.get()));
        m_registered_input.unregister();
    }

    /// Pins the caller's output histogram, so values(out) into it copies at
    /// full bandwidth. Same rules as register_input_buffer().
    void register_output_buffer(void *ptr, size_t bytes) {
        CUDA_CHECK(cudaStreamSynchronize(compute_stream.get()));
        m_registered_output.unregister();
        m_registered_output = HostRegistration(ptr, bytes);
    }

    /// Releases the buffer pinned by register_output_buffer().
    void unregister_output_buffer() {
        CUDA_CHECK(cudaStreamSynchronize(compute_stream.get()));
        m_registered_output.unregister();
    }

    /// Queues one batch of frames, shape (n, rows, cols) with n <= batch_size,
    /// and returns without waiting. Don't modify the frames until flush() or
    /// values() has returned. Throws std::invalid_argument on a wrong shape.
    void submit_batch(NDView<const InputType, 3> frames) {
        auto frame_shape = frames.shape();
        if (static_cast<size_t>(frame_shape[0]) > m_batch_size ||
            static_cast<size_t>(frame_shape[1]) != nrows ||
            static_cast<size_t>(frame_shape[2]) != ncols) {
            throw std::invalid_argument("PixelHistogramCUDA::submit_batch: "
                                        "frame shape does not match histogram");
        }

        size_t batch_size =
            frame_shape[0]; // can be less than m_batch_size for the last batch

        const int slot = m_next_batch++ % NUM_SLOTS;

        // pointer to the first frame in the batch
        const InputType *h_frames_batch = frames.data();

        // Make sure `copy_stream` waits on any kernel (using `slot`) to finish
        // execution
        CUDA_CHECK(
            cudaStreamWaitEvent(copy_stream.get(), kernel_done[slot].get(), 0));

        // Copy frames to device
        const size_t npix = nrows * ncols;
        CUDA_CHECK(cudaMemcpyAsync(d_frames_batch[slot].get(), h_frames_batch,
                                   npix * batch_size * sizeof(InputType),
                                   cudaMemcpyHostToDevice, copy_stream.get()));

        // Record the copy_done event (in the copy_stream) right after the copy
        // command
        CUDA_CHECK(cudaEventRecord(copy_done[slot].get(), copy_stream.get()));
        // Launch kernel to fill histogram
        const dim3 blockDim(BLOCK_SIZE);
        const dim3 gridDim((npix + BLOCK_SIZE - 1) / BLOCK_SIZE);

        // compute_stream waits until copy is done and slot is filled
        CUDA_CHECK(
            cudaStreamWaitEvent(compute_stream.get(), copy_done[slot].get()));

        // Clear CUDA's per-thread last error first: one left by a call
        // whose result nobody checked (the deleters ignore theirs) would
        // otherwise be reported as this launch's error.
        cudaGetLastError();
        if (m_window) {
            detail::fill_window<<<gridDim, blockDim, WINDOW_SHMEM_BYTES,
                                  compute_stream.get()>>>(
                d_frames_batch[slot].get(), d_histogram.get(), d_centre.get(),
                npix, m_nbins, m_xmin, m_xmax, m_scale, batch_size);
        } else {
            detail::fill_direct<<<gridDim, blockDim, 0, compute_stream.get()>>>(
                d_frames_batch[slot].get(), d_histogram.get(), npix, m_nbins,
                m_xmin, m_xmax, m_scale, batch_size);
        }
        const cudaError_t launch_error = cudaGetLastError();
        // Recorded even if the launch failed, so the next copy into this
        // slot still waits for everything queued before it.
        CUDA_CHECK(
            cudaEventRecord(kernel_done[slot].get(), compute_stream.get()));
        CUDA_CHECK(launch_error);
    }

    /// Queues frames of shape (n_frames, rows, cols) in batches of
    /// batch_size, and returns without waiting (see submit_batch()).
    void fill_histogram(NDView<const InputType, 3> frames) {
        auto N_frames = static_cast<size_t>(frames.shape(0));
        for (size_t i = 0; i < N_frames; i += m_batch_size) {
            size_t batch_end = std::min(i + m_batch_size, N_frames);
            NDView<const InputType, 3> batch = frames.sub_view(i, batch_end);
            submit_batch(batch);
        }
    }

    /// Blocks until every queued frame has been histogrammed.
    void flush() const {
        CUDA_CHECK(cudaStreamSynchronize(copy_stream.get()));
        CUDA_CHECK(cudaStreamSynchronize(compute_stream.get()));
    }

    /// Writes the histogram of every frame queued so far into out, shape
    /// (rows, cols, n_bins), capped at StorageType's maximum. Waits for the
    /// queued work; filling can continue afterwards. A registered out
    /// (register_output_buffer()) copies fastest. Throws
    /// std::invalid_argument on a wrong shape.
    void values(NDView<StorageType, 3> out) const {
        // out is C-order, so its shape fixes the layout: pixel-major.
        if (out.shape(0) !=
                static_cast<ssize_t>(nrows) // slowest index in `out`
            || out.shape(1) != static_cast<ssize_t>(ncols) ||
            out.shape(2) != static_cast<ssize_t>(m_nbins) // fastest index
        ) {
            throw std::invalid_argument(
                "The handed buffer has the incorrect shape."
                " Output histogram ought to be of shape {" +
                std::to_string(nrows) + ", " + std::to_string(ncols) + ", " +
                std::to_string(m_nbins) + "}.");
        }
        const size_t total = nrows * ncols * m_nbins;

        // On the compute stream, so after every kernel queued so far.
        if constexpr (std::is_same_v<StorageType, DeviceStorageType>) {
            // The cap is the type's own maximum: nothing to cap or narrow,
            // copy the accumulator as it is.
            CUDA_CHECK(cudaMemcpyAsync(
                out.data(), d_histogram.get(), total * sizeof(StorageType),
                cudaMemcpyDeviceToHost, compute_stream.get()));
            CUDA_CHECK(cudaStreamSynchronize(compute_stream.get()));
        } else {
            // Cap the counts at what StorageType holds and narrow them, on
            // the GPU; the accumulator is already in the output layout.
            auto d_capped =
                make_async_buffer<StorageType>(total, compute_stream.get());
            const dim3 blockDim(BLOCK_SIZE);
            const dim3 gridDim(static_cast<unsigned int>(
                (total + BLOCK_SIZE - 1) / BLOCK_SIZE));
            cudaGetLastError(); // clear a stale error, as in submit_batch()
            detail::cap_counts<<<gridDim, blockDim, 0, compute_stream.get()>>>(
                d_histogram.get(), d_capped.get(), total, count_cap());
            CUDA_CHECK(cudaGetLastError());
            CUDA_CHECK(cudaMemcpyAsync(
                out.data(), d_capped.get(), total * sizeof(StorageType),
                cudaMemcpyDeviceToHost, compute_stream.get()));
            CUDA_CHECK(cudaStreamSynchronize(compute_stream.get()));
        }
    }

    /// Same as values(out), into a new array.
    NDArray<StorageType, 3> values() const {
        NDArray<StorageType, 3> h_histogram_out(
            {static_cast<ssize_t>(nrows), static_cast<ssize_t>(ncols),
             static_cast<ssize_t>(m_nbins)}); // Pixel-Major layout
        values(h_histogram_out.view());

        return h_histogram_out;
    }

    /// Histogram of one pixel, shape (n_bins,), capped like values(). For
    /// many pixels, call values() once. Throws std::out_of_range outside the
    /// frame.
    NDArray<StorageType, 1> values_pixel(size_t row, size_t col) const {
        if (row >= nrows || col >= ncols)
            throw std::out_of_range(
                "Incorrect pixel coordinate; out-of bound access");
        NDArray<DeviceStorageType, 1> pixel_hist(
            {static_cast<ssize_t>(m_nbins)});

        auto p = row * ncols + col;

        // One pixel's bins are contiguous in the device accumulator.
        CUDA_CHECK(
            cudaMemcpyAsync(pixel_hist.data(), d_histogram.get() + p * m_nbins,
                            m_nbins * sizeof(DeviceStorageType),
                            cudaMemcpyDeviceToHost, compute_stream.get()));

        CUDA_CHECK(cudaStreamSynchronize(compute_stream.get()));

        NDArray<StorageType, 1> pixel_hist_capped(
            {static_cast<ssize_t>(m_nbins)});
        auto cap = count_cap();
        for (ssize_t i = 0; i < static_cast<ssize_t>(m_nbins); ++i)
            pixel_hist_capped[i] =
                static_cast<StorageType>(std::min(cap, pixel_hist[i]));
        return pixel_hist_capped;
    }

    /// n_bins + 1 bin edges from xmin to xmax, computed like PixelHistogram.
    NDArray<AxisType, 1> bin_edges() const {
        NDArray<AxisType, 1> edges({static_cast<ssize_t>(m_nbins + 1)});
        const AxisType range = m_xmax - m_xmin;
        for (ssize_t i = 0; i <= static_cast<ssize_t>(m_nbins); ++i) {
            edges[i] = m_xmin + (static_cast<AxisType>(i) * range) /
                                    static_cast<AxisType>(m_nbins);
        }
        return edges;
    }

    /// n_bins bin centres, the midpoints of bin_edges().
    NDArray<AxisType, 1> bin_centers() const {
        NDArray<AxisType, 1> edges = bin_edges();
        NDArray<AxisType, 1> centers({static_cast<ssize_t>(m_nbins)});
        std::transform(edges.begin(), std::prev(edges.end()), edges.begin() + 1,
                       centers.begin(), [](auto lhs, auto rhs) {
                           return (lhs + rhs) / AxisType(2);
                       });
        return centers;
    }

    size_t rows() const noexcept { return nrows; }
    size_t cols() const noexcept { return ncols; }
    size_t n_bins() const noexcept { return m_nbins; }
    size_t batch_size() const noexcept { return m_batch_size; }
    /// true if the fill kernel is fill_window, false for fill_direct.
    bool window() const noexcept { return m_window; }
};

} // namespace aare::cuda
