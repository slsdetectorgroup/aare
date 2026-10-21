// SPDX-License-Identifier: MPL-2.0
#pragma once
#include "aare/NDArray.hpp"
#include "aare/NDView.hpp"
#include "aare/RAII_device.hpp"
#include "aare/hist/pixelhistogram_kernel.cuh"
#include "aare/utils/cuda_check.cuh"
#include <algorithm>
#include <array>
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

template <typename StorageType = uint16_t,
          typename DeviceStorageType = uint32_t, typename InputType = float,
          int BLOCK_SIZE = 128>
class PixelHistogramCUDA {
#ifdef AARE_TESTS
    // Writes a known pattern into d_histogram to test values() without the
    // fill kernel (PixelHistogramCUDA_testimpl.cu).
    FRIEND_TEST(known_pattern_transpose_and_cap)
#endif

    // Frames are copied to the device as InputType, as the caller provides
    // them. The histogram axis (range, bin edges, the bin arithmetic) must be
    // floating point: an integral input gets a float axis, converted in the
    // kernel. That conversion is exact up to 16-bit integers.
    static_assert(std::is_floating_point_v<InputType> ||
                      (std::is_integral_v<InputType> && sizeof(InputType) <= 2),
                  "PixelHistogramCUDA: InputType must be floating point or an "
                  "integer of at most 16 bits");

  public:
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
    size_t m_next_batch;
    mutable size_t m_processed_batches; // flush() touches it.

    static constexpr int NUM_SLOTS =
        2; // Number of concurrent batches to process

    // The order of the declarations below is important. It conditions the order
    // of their destruction.
    HostRegistration m_registered_input{};
    HostRegistration m_registered_output{};

    // Device buffers for frames and histogram
    DeviceBuffer<DeviceStorageType> d_histogram;
    std::array<DeviceBuffer<InputType>, NUM_SLOTS> d_frames_batch;

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
                             AxisType xmin, AxisType xmax, size_t batch_size) {
        if (rows < 1 || cols < 1 || n_bins < 1) {
            throw std::invalid_argument(
                "PixelHistogramCUDA requires positive rows, cols and bins");
        }
        if (!(xmax > xmin)) {
            throw std::invalid_argument(
                "PixelHistogramCUDA requires xmax > xmin");
        }
        if (batch_size < 1) {
            throw std::invalid_argument(
                "PixelHistogramCUDA requires batch_size >= 1");
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
    PixelHistogramCUDA(size_t rows, size_t cols, size_t n_bins, AxisType xmin,
                       AxisType xmax, size_t batch_size = 1)
        : nrows{check_args(rows, cols, n_bins, xmin, xmax, batch_size)},
          ncols{cols}, m_nbins{n_bins}, m_xmin{xmin}, m_xmax{xmax},
          m_scale{m_nbins / (m_xmax - m_xmin)}, m_batch_size{batch_size},
          m_next_batch{0}, m_processed_batches{0},
          d_histogram{nrows * ncols * m_nbins}, copy_stream{make_stream()},
          compute_stream{make_stream()} {

        // Construct events
        for (int i = 0; i < NUM_SLOTS; ++i) {
            copy_done[i] = make_event();
            kernel_done[i] = make_event();
        }

        CUDA_CHECK(cudaMemsetAsync(d_histogram.get(), 0, d_histogram.bytes(),
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

    void register_input_buffer(void *ptr, size_t bytes) {
        CUDA_CHECK(cudaStreamSynchronize(
            copy_stream.get())); // Wait for copies to end in the stream
        m_registered_input.unregister();
        m_registered_input = HostRegistration(ptr, bytes);
    }

    void unregister_input_buffer() {
        CUDA_CHECK(cudaStreamSynchronize(copy_stream.get()));
        m_registered_input.unregister();
    }

    // Pins the caller's output histogram, so values(out) into it copies at
    // DMA speed. The D2H copy runs on the compute stream.
    void register_output_buffer(void *ptr, size_t bytes) {
        CUDA_CHECK(cudaStreamSynchronize(compute_stream.get()));
        m_registered_output.unregister();
        m_registered_output = HostRegistration(ptr, bytes);
    }

    void unregister_output_buffer() {
        CUDA_CHECK(cudaStreamSynchronize(compute_stream.get()));
        m_registered_output.unregister();
    }

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

        detail::fill_bin_major<<<gridDim, blockDim, 0, // let's not use shmem at
                                                       // first
                                 compute_stream.get()>>>(
            d_frames_batch[slot].get(), d_histogram.get(), npix, m_nbins,
            m_xmin, m_xmax, m_scale, batch_size);
        CUDA_CHECK(cudaGetLastError());

        CUDA_CHECK(
            cudaEventRecord(kernel_done[slot].get(), compute_stream.get()));
    }

    void fill_histogram(NDView<const InputType, 3> frames) {
        auto N_frames = static_cast<size_t>(frames.shape(0));
        for (size_t i = 0; i < N_frames; i += m_batch_size) {
            size_t batch_end = std::min(i + m_batch_size, N_frames);
            NDView<const InputType, 3> batch = frames.sub_view(i, batch_end);
            submit_batch(batch);
        }
    }

    void flush() const {
        CUDA_CHECK(cudaStreamSynchronize(copy_stream.get()));
        CUDA_CHECK(cudaStreamSynchronize(compute_stream.get()));
        m_processed_batches = m_next_batch;
    }

    // In theory, this fct captures the histogram built from batches processed
    // so far i.e. possibly runs between 2 consecutive batches and before all
    // frames where processed...
    void values(NDView<StorageType, 3> out) const {
        // Make sure the handle points to a well-shaped owning NDArray in
        // Pixel-Major layout
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
        const size_t npix = nrows * ncols;

        // launch a kernel that transposes the d_histogram from BIN-Major to
        // PIXEL-major
        auto d_histogram_pixMajor = make_async_buffer<StorageType>(
            npix * m_nbins, compute_stream.get());

        // Cap (per-bin) counts and transpose the device histogram to comply
        // with host format Option A : 1D config + NO shared mem
        const size_t total = npix * m_nbins;
        const dim3 blockDim(BLOCK_SIZE);
        const dim3 gridDim(
            static_cast<unsigned int>((total + BLOCK_SIZE - 1) / BLOCK_SIZE));
        detail::
            bin_to_pixel_major<<<gridDim, blockDim, 0, compute_stream.get()>>>(
                d_histogram.get(), d_histogram_pixMajor.get(), npix, m_nbins,
                count_cap());
        CUDA_CHECK(cudaGetLastError());

        // // Option B (optimized): use 2d tiled transpose with shared memory
        // const dim3 blockDim(32, 8); // threadBlock of 256 threads
        // const dim3 gridDim( (npix + 31) / 32, (m_nbins + 31) / 32 ); //
        // Tile_X = 32 and Tile_Y = 32
        // TODO

        // Copy a capture of the histogram back to host using the compute_stream
        CUDA_CHECK(cudaMemcpyAsync(out.data(), d_histogram_pixMajor.get(),
                                   npix * m_nbins * sizeof(StorageType),
                                   cudaMemcpyDeviceToHost,
                                   compute_stream.get()));
        // wait for all ops pipelined in compute_stream to finish on the GPU
        CUDA_CHECK(cudaStreamSynchronize(compute_stream.get()));
    }

    NDArray<StorageType, 3> values() const {
        NDArray<StorageType, 3> h_histogram_out(
            {static_cast<ssize_t>(nrows), static_cast<ssize_t>(ncols),
             static_cast<ssize_t>(m_nbins)}); // Pixel-Major layout
        values(h_histogram_out.view());

        return h_histogram_out;
    }

    // Lazy convenience function (slow):
    // Needs transpose form Bin-Major to Pixel-Major
    NDArray<StorageType, 1> values_pixel(size_t row, size_t col) const {
        if (row >= nrows || col >= ncols)
            throw std::out_of_range(
                "Incorrect pixel coordinate; out-of bound access");
        NDArray<DeviceStorageType, 1> pixel_hist(
            {static_cast<ssize_t>(m_nbins)});

        auto p = row * ncols + col;

        CUDA_CHECK(cudaMemcpy2DAsync(
            /*dst     =*/pixel_hist.data(),
            /* dpitch =*/sizeof(DeviceStorageType), // destination stride
            /* src    =*/d_histogram.get() +
                p, // d_histogram has a Bin-Major layout
            /* spitch =*/nrows * ncols *
                sizeof(DeviceStorageType),          // source stride
            /* width  =*/sizeof(DeviceStorageType), // in bytes
            /* height =*/m_nbins,                   // number of rows
            /* kind   =*/cudaMemcpyDeviceToHost,
            /* stream =*/compute_stream.get()));

        CUDA_CHECK(cudaStreamSynchronize(compute_stream.get()));

        NDArray<StorageType, 1> pixel_hist_capped(
            {static_cast<ssize_t>(m_nbins)});
        auto cap = count_cap();
        for (ssize_t i = 0; i < static_cast<ssize_t>(m_nbins); ++i)
            pixel_hist_capped[i] =
                static_cast<StorageType>(std::min(cap, pixel_hist[i]));
        return pixel_hist_capped;
    }

    NDArray<AxisType, 1> bin_edges() const {
        NDArray<AxisType, 1> edges({static_cast<ssize_t>(m_nbins + 1)});
        const AxisType range = m_xmax - m_xmin;
        for (ssize_t i = 0; i <= static_cast<ssize_t>(m_nbins); ++i) {
            edges[i] = m_xmin + (static_cast<AxisType>(i) * range) /
                                    static_cast<AxisType>(m_nbins);
        }
        return edges;
    }

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
};

} // namespace aare::cuda
