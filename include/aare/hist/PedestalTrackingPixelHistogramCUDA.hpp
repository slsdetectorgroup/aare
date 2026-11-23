// SPDX-License-Identifier: MPL-2.0
#pragma once
#include "aare/FastPedestal.hpp"
#include "aare/hist/HistogramPipelineCUDA.hpp"
#include <cstdint>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

namespace aare::cuda {

/// Per-pixel histograms of pedestal-subtracted frames, with the pedestal
/// tracked during the fill, on the GPU: the GPU port of
/// PedestalTrackingPixelHistogram, with the same results.
///
/// 1. Seeding: push_pedestal_no_update() with exactly n_samples raw frames
///    (default 1000, as the CPU class) accumulates the pedestal on the host,
///    and uploads it to the GPU with the last one.
/// 2. update_mean() sets each pixel's sigma from its pedestal. It stays fixed
///    until the next update_mean() call.
/// 3. fill_histogram() histograms float(raw) - mean of every value and, for a
///    value with |residual| < n_sigma * sigma, updates the pixel's pedestal
///    with raw (exponential moving average with weight 1 / n_samples), frame
///    by frame. n_sigma <= 0 disables the tracking.
///
/// Before seeding is complete the mean is 0, and before update_mean() sigma
/// is 0, which disables the tracking: as on the CPU.
///
/// With n_samples = 1000, the histograms, pedestal_mean() and pedestal_std()
/// equal the CPU class's bit for bit on the same frames.
///
/// The pipeline, the fill and readout methods and the pinning methods are
/// those of PixelHistogramCUDA (detail::HistogramPipeline). Frames are
/// uint16; the axis is float, as on the CPU.
///
/// GPU memory: as PixelHistogramCUDA, plus 24 bytes per pixel for the
/// pedestal (and 1 for the mask).
///
/// Not thread-safe: one object must not be used from two threads at once.
///
/// @tparam StorageType type of the counts values() returns, capped at its
///         maximum. The CPU class has uint16_t.
/// @tparam DeviceStorageType unsigned integer type of the GPU counters.
/// @tparam BLOCK_SIZE threads per block of the fill kernels.
template <typename StorageType = uint16_t,
          typename DeviceStorageType = uint32_t, int BLOCK_SIZE = 128>
class PedestalTrackingPixelHistogramCUDA
    : public detail::HistogramPipeline<detail::TrackingFill, StorageType,
                                       DeviceStorageType, BLOCK_SIZE> {
    using Base = detail::HistogramPipeline<detail::TrackingFill, StorageType,
                                           DeviceStorageType, BLOCK_SIZE>;
    static constexpr const char *NAME = "PedestalTrackingPixelHistogramCUDA";

    // Accumulates the seed frames on the host. Once it is ready the GPU holds
    // the pedestal; this copy is not updated by the fill.
    FastPedestal<float> m_seed;

    static uint32_t check_n_samples(uint32_t n_samples) {
        if (n_samples < 1)
            throw std::invalid_argument(std::string(NAME) +
                                        " requires n_samples >= 1");
        return n_samples;
    }

    void check_frame_shape(ssize_t rows, ssize_t cols,
                           const char *method) const {
        if (rows != static_cast<ssize_t>(this->nrows) ||
            cols != static_cast<ssize_t>(this->ncols))
            throw std::invalid_argument(std::string(NAME) + "::" + method +
                                        ": shape does not match the histogram");
    }

    // Asynchronous copy of a host array into device memory on the compute
    // stream, then a wait: the host arrays here are temporaries.
    template <typename T> void upload(T *dst, const T *src, size_t n) const {
        CUDA_CHECK(cudaMemcpyAsync(dst, src, n * sizeof(T),
                                   cudaMemcpyHostToDevice,
                                   this->compute_stream.get()));
        CUDA_CHECK(cudaStreamSynchronize(this->compute_stream.get()));
    }

    // Device array of one value per pixel, after every queued fill.
    NDArray<float, 2> download(const DeviceBuffer<float> &src) const {
        NDArray<float, 2> out({static_cast<ssize_t>(this->nrows),
                               static_cast<ssize_t>(this->ncols)});
        CUDA_CHECK(cudaMemcpyAsync(out.data(), src.get(), src.bytes(),
                                   cudaMemcpyDeviceToHost,
                                   this->compute_stream.get()));
        CUDA_CHECK(cudaStreamSynchronize(this->compute_stream.get()));
        return out;
    }

  public:
    /// Type of the range, the bin edges and the residuals.
    using AxisType = float;
    /// Type of the frames.
    using FrameType = uint16_t;

    /// n_bins equal bins of the residual over [xmin, xmax). Throws
    /// std::invalid_argument for zero rows, cols, n_bins, batch_size or
    /// n_samples, a non-finite range, xmax <= xmin, or a mask of another
    /// shape.
    ///
    /// @param n_sigma gate of the pedestal update, see the class description.
    ///        Default 1, as the CPU class.
    /// @param mask true for pixels to skip entirely: no histogram, no
    ///        tracking. Copied.
    /// @param n_samples seed frames, and the weight 1 / n_samples of a value
    ///        in the pedestal update. The CPU class has 1000.
    /// @param batch_size, window as for PixelHistogramCUDA.
    PedestalTrackingPixelHistogramCUDA(
        size_t rows, size_t cols, size_t n_bins, AxisType xmin, AxisType xmax,
        AxisType n_sigma = 1.0f,
        std::optional<NDView<const bool, 2>> mask = std::nullopt,
        uint32_t n_samples = 1000, size_t batch_size = 1024, bool window = true)
        : Base(NAME, rows, cols, n_bins, xmin, xmax, batch_size, window),
          m_seed(static_cast<uint32_t>(rows), static_cast<uint32_t>(cols),
                 check_n_samples(n_samples)) {
        const size_t npix = this->nrows * this->ncols;
        auto &state = this->m_state;
        const cudaStream_t stream = this->compute_stream.get();
        // Mean and sigma 0 until seeded: residuals are the raw values, and
        // no tracking.
        CUDA_CHECK(
            cudaMemsetAsync(state.sum.get(), 0, state.sum.bytes(), stream));
        CUDA_CHECK(
            cudaMemsetAsync(state.sum2.get(), 0, state.sum2.bytes(), stream));
        CUDA_CHECK(
            cudaMemsetAsync(state.mean.get(), 0, state.mean.bytes(), stream));
        CUDA_CHECK(
            cudaMemsetAsync(state.sigma.get(), 0, state.sigma.bytes(), stream));
        // As FastPedestal: 1/n in double, multiplied, never divided by.
        this->m_params = {1.0 / static_cast<double>(n_samples), n_sigma};

        if (mask) {
            check_frame_shape(mask->shape(0), mask->shape(1), "mask");
            std::vector<uint8_t> h_mask(npix);
            for (size_t i = 0; i < npix; ++i)
                h_mask[i] = (*mask)[static_cast<ssize_t>(i)] ? 1 : 0;
            state.mask = DeviceBuffer<uint8_t>(npix);
            upload(state.mask.get(), h_mask.data(), npix);
        }
    }

    /// Adds one raw frame to the pedestal seed. The n_samples-th frame
    /// completes the seed and uploads it to the GPU, after the fills queued
    /// so far. Throws std::invalid_argument on a wrong shape and
    /// std::runtime_error once the seed is complete.
    void push_pedestal_no_update(NDView<const FrameType, 2> frame) {
        check_frame_shape(frame.shape(0), frame.shape(1),
                          "push_pedestal_no_update");
        if (m_seed.ready())
            throw std::runtime_error(
                std::string(NAME) + "::push_pedestal_no_update: the pedestal " +
                "already has its " + std::to_string(m_seed.n_samples()) +
                " frames");
        m_seed.add_init_frame(frame);
        if (!m_seed.ready())
            return;

        const auto sum = m_seed.get_sum();
        const auto sum2 = m_seed.get_sum2();
        const auto mean = m_seed.mean();
        const size_t npix = this->nrows * this->ncols;
        auto &state = this->m_state;
        upload(state.sum.get(), sum.data(), npix);
        upload(state.sum2.get(), sum2.data(), npix);
        upload(state.mean.get(), mean.data(), npix);
    }

    /// Sets each pixel's sigma from its pedestal as tracked so far, after the
    /// fills queued so far. The fill uses it until the next call. Throws
    /// std::runtime_error before the seed is complete.
    void update_mean() {
        if (!m_seed.ready())
            throw std::runtime_error(
                std::string(NAME) + "::update_mean: the pedestal needs " +
                std::to_string(m_seed.n_samples()) + " frames, got " +
                std::to_string(m_seed.cur_samples()));
        const size_t npix = this->nrows * this->ncols;
        const auto &state = this->m_state;
        cudaGetLastError(); // clear a stale error, as in submit_batch()
        detail::tracking_sigma<<<static_cast<unsigned>((npix + BLOCK_SIZE - 1) /
                                                       BLOCK_SIZE),
                                 BLOCK_SIZE, 0, this->compute_stream.get()>>>(
            state.sum.get(), state.sum2.get(), state.sigma.get(), npix,
            this->m_params.inv_samples);
        CUDA_CHECK(cudaGetLastError());
    }

    /// Pedestal mean of every pixel, shape (rows, cols), after every queued
    /// fill. Throws std::runtime_error before the seed is complete.
    NDArray<AxisType, 2> pedestal_mean() const {
        if (!m_seed.ready())
            throw std::runtime_error(std::string(NAME) +
                                     "::pedestal_mean: the pedestal is not "
                                     "seeded yet");
        return download(this->m_state.mean);
    }

    /// Sigma of every pixel as set by the last update_mean(), shape
    /// (rows, cols); 0 before.
    NDArray<AxisType, 2> pedestal_std() const {
        return download(this->m_state.sigma);
    }

    /// Gate of the pedestal update; applies to the batches queued after the
    /// change.
    AxisType n_sigma() const noexcept { return this->m_params.n_sigma; }
    void set_n_sigma(AxisType n_sigma) noexcept {
        this->m_params.n_sigma = n_sigma;
    }

    /// Seed frames needed, and the weight 1 / n_samples of the update.
    uint32_t n_samples() const noexcept { return m_seed.n_samples(); }
    /// Seed frames pushed so far, at most n_samples().
    uint32_t pedestal_frames() const noexcept { return m_seed.cur_samples(); }
};

} // namespace aare::cuda
