// SPDX-License-Identifier: MPL-2.0
#pragma once
#include "aare/File.hpp"
#include "aare/Frame.hpp"
#include "aare/NDArray.hpp"
#include "aare/NDView.hpp"
#include <algorithm>
#include <cstddef>
#include <sys/types.h>

namespace aare {

/**
 * @brief Complete state of a FastPedestal as plain data.
 *
 * Returned by FastPedestal::state() and accepted by FastPedestal::set_state()
 * and the FastPedestal constructor, so that a pedestal can be stored and
 * later continued exactly where it stopped. The state does not depend on the
 * PEDESTAL_TYPE of the pedestal it was taken from: the moments are always
 * double and the cached mean is recomputed from them.
 *
 * sum and sum2 accumulate the pixel values and their squares over the
 * initialization frames. Once the pedestal is ready (cur_samples equals
 * n_samples) each new value replaces 1 / n_samples of them, so the mean is
 * sum / n_samples and the second moment is sum2 / n_samples.
 */
struct FastPedestalState {
    /// Number of initialization frames and reciprocal weight of later values
    uint32_t n_samples{};
    /// Initialization frames accumulated so far, at most n_samples
    uint32_t cur_samples{};
    /// Per-pixel accumulator of the values, shape (rows, cols)
    NDArray<double, 2> sum;
    /// Per-pixel accumulator of the squared values, shape (rows, cols)
    NDArray<double, 2> sum2;
};

/**
 * @brief Maintain per-pixel mean and population standard deviation.
 *
 * Initialization accumulates exactly n_samples frames. Subsequent push_ema()
 * update the exponential moving average initialized with the mean using
 * a smoothing factor of (1/ n_samples). Internal moments are stored
 * in double precision. Variance is a private double-precision intermediate.
 * @tparam PEDESTAL_TYPE Type returned for the mean and standard deviation.
 */
template <typename PEDESTAL_TYPE> class FastPedestal {

    // Did we accumulate enough samples and updated the mean?
    bool m_ready = false;

    uint32_t m_rows;
    uint32_t m_cols;

    uint32_t m_samples;
    double m_inv_samples;       // precompute 1/m_samples for faster division
    uint32_t m_cur_samples = 0; // number of samples accumulated so far

    // For cache locality we want to keep sum and sum2 close. Should
    // improve performance for random access.
    struct Entry {
        double sum;
        double sum2;
    };
    NDArray<Entry, 2> m_sum;

    // Cache mean since it is used over and over in the ClusterFinder
    // This optimization is related to the access pattern of the ClusterFinder
    // Relies on having more reads than pushes to the pedestal
    // But also makes sense when subtracting the pedestal from the frame
    NDArray<PEDESTAL_TYPE, 2> m_mean;

    // Helper function to convert row and column indices to a flat index
    // used to provide both row column and flat index access to the pedestal
    size_t rc_to_index(uint32_t row, uint32_t col) const {
        return (static_cast<std::size_t>(row) * m_cols) + col;
    }

  public:
    /**
     * @brief Construct an empty pedestal that becomes ready after n_samples
     * initialization frames.
     * @param rows Number of image rows.
     * @param cols Number of image columns.
     * @param n_samples Number of initialization frames and reciprocal of the
     * weight assigned to each subsequent value.
     * @throws std::runtime_error if rows, cols, or n_samples is zero.
     */
    FastPedestal(uint32_t rows, uint32_t cols, uint32_t n_samples = 1000)
        : m_rows(rows), m_cols(cols), m_samples(n_samples),
          m_inv_samples(1.0 / n_samples), m_sum({rows, cols}, Entry{0, 0}),
          m_mean({rows, cols}, PEDESTAL_TYPE(0)) {
        if (!(rows > 0 && cols > 0 && n_samples > 0)) {
            throw std::runtime_error(
                fmt::format("Invalid parameters for FastPedestal: rows={}, "
                            "cols={}, n_samples={} need to be positive",
                            rows, cols, n_samples));
        }
    }

    /**
     * @brief Construct a pedestal from a state, taking the image shape and
     * n_samples from it.
     * @throws std::runtime_error if the state is empty or not valid, see
     * set_state().
     */
    explicit FastPedestal(const FastPedestalState &state)
        : FastPedestal(static_cast<uint32_t>(state.sum.shape(0)),
                       static_cast<uint32_t>(state.sum.shape(1)),
                       state.n_samples) {
        set_state(state);
    }

    ~FastPedestal() = default;

    /**
     * @brief Return a copy of the complete state.
     *
     * A pedestal given this state continues exactly as this one would,
     * whether it is still initializing or ready.
     */
    FastPedestalState state() const {
        FastPedestalState state{m_samples, m_cur_samples,
                                NDArray<double, 2>({m_rows, m_cols}),
                                NDArray<double, 2>({m_rows, m_cols})};
        for (ssize_t i = 0; i < m_sum.size(); ++i) {
            state.sum[i] = m_sum[i].sum;
            state.sum2[i] = m_sum[i].sum2;
        }
        return state;
    }

    /**
     * @brief Replace the complete state, including n_samples.
     *
     * The cached mean is recomputed, so the state can come from a pedestal
     * of another PEDESTAL_TYPE.
     * @param state State whose sum and sum2 have the shape of this pedestal,
     * with n_samples positive and cur_samples at most n_samples. The values
     * of sum and sum2 are not checked.
     * @throws std::runtime_error if the state is not valid. The pedestal is
     * then left unchanged.
     */
    void set_state(const FastPedestalState &state) {
        const Shape<2> shape{m_rows, m_cols};
        if (state.sum.shape() != shape || state.sum2.shape() != shape) {
            throw std::runtime_error(fmt::format(
                "FastPedestal state has sum of shape ({}, {}) and sum2 of "
                "shape ({}, {}), expected ({}, {})",
                state.sum.shape(0), state.sum.shape(1), state.sum2.shape(0),
                state.sum2.shape(1), m_rows, m_cols));
        }
        if (state.n_samples == 0 || state.cur_samples > state.n_samples) {
            throw std::runtime_error(fmt::format(
                "Invalid FastPedestal state: n_samples={} must be positive "
                "and cur_samples={} at most n_samples",
                state.n_samples, state.cur_samples));
        }

        m_samples = state.n_samples;
        m_inv_samples = 1.0 / m_samples;
        m_cur_samples = state.cur_samples;
        m_ready = m_cur_samples == m_samples;
        for (ssize_t i = 0; i < m_sum.size(); ++i) {
            m_sum[i] = Entry{state.sum[i], state.sum2[i]};
        }
        // The mean is only written once the pedestal is ready
        if (m_ready) {
            update_mean();
        } else {
            m_mean = PEDESTAL_TYPE(0);
        }
    }

    /**
     * @brief Return a non-owning view of the cached mean.
     * @throws std::runtime_error if ready() is false.
     * @note The caller must treat the data as read-only and must not retain the
     * view after this object is destroyed, moved, or assigned.
     */
    NDView<const PEDESTAL_TYPE, 2> view() const {
        if (!ready()) {
            throw std::runtime_error(
                "Pedestal is not ready, cannot return view");
        }
        return m_mean.view();
    }

    /**
     * @brief Return a copy of the cached mean.
     * @throws std::runtime_error if ready() is false.
     */
    NDArray<PEDESTAL_TYPE, 2> mean() const {
        if (!ready()) {
            throw std::runtime_error(
                "Pedestal is not ready, cannot return mean");
        }
        return m_mean;
    }

    /**
     * @brief Return the cached mean at (row, col).
     * @throws std::runtime_error if ready() is false or either index is out of
     * range.
     */
    PEDESTAL_TYPE mean(uint32_t row, uint32_t col) const {
        if (!ready()) {
            throw std::runtime_error(
                "Pedestal is not ready, cannot return mean");
        }
        if (row >= m_rows || col >= m_cols) {
            throw std::runtime_error(
                fmt::format("Invalid indices for FastPedestal mean: row={}, "
                            "col={} must be in [0, {}), [0, {})",
                            row, col, m_rows, m_cols));
        }
        return m_mean(row, col);
    }

    /**
     * @brief Return the cached mean at a flat row-major index.
     * @pre ready() is true, the cache is current, and index is valid; the index
     * is not checked.
     */
    PEDESTAL_TYPE mean_unchecked(ssize_t index) const { return m_mean[index]; }

    /**
     * @brief Calculate and return the population standard deviation of every
     * pixel.
     * @throws std::runtime_error if ready() is false.
     */
    NDArray<PEDESTAL_TYPE, 2> std() const {
        if (!ready()) {
            throw std::runtime_error(
                "Pedestal is not ready, cannot return std");
        }
        NDArray<PEDESTAL_TYPE, 2> res({m_rows, m_cols});
        for (ssize_t i = 0; i < m_sum.size(); ++i) {
            res[i] = std_unchecked(i);
        }
        return res;
    }

    /**
     * @brief Calculate the population standard deviation at (row, col).
     * @throws std::runtime_error if ready() is false or either index is out of
     * range.
     */
    PEDESTAL_TYPE std(const uint32_t row, const uint32_t col) const {
        if (!ready()) {
            throw std::runtime_error(
                "Pedestal is not ready, cannot return std");
        }
        if (row >= m_rows || col >= m_cols) {
            throw std::runtime_error(
                fmt::format("Invalid indices for FastPedestal std: row={}, "
                            "col={} must be in [0, {}), [0, {})",
                            row, col, m_rows, m_cols));
        }
        return std_unchecked(rc_to_index(row, col));
    }

    /**
     * @brief Calculate the population standard deviation at a flat row-major
     * index.
     * @pre ready() is true and index is valid; the index is not checked.
     */
    PEDESTAL_TYPE std_unchecked(ssize_t index) const {
        return std::sqrt(variance_unchecked(index));
    }

    /**
     * @brief Return whether initialization is complete (cur_samples() equals
     * n_samples()).
     */
    bool ready() const { return m_ready; }

    /**
     * @brief Return the stored number of accumulated initialization frames.
     * @note The value is in [0, n_samples] and does not change during
     * steady-state pushes.
     */
    uint32_t cur_samples() const { return m_cur_samples; }

    /**
     * @brief Zero the moments and cached mean, and mark the pedestal not ready.
     */
    void clear() {
        m_sum = Entry{0., 0.};
        m_mean = PEDESTAL_TYPE(0.);
        m_cur_samples = 0;
        m_ready = false;
    }

    /**
     * @brief Update every pixel using the steady-state exponential estimator,
     * giving the new value weight 1 / n_samples.
     * @param frame Frame whose shape must exactly match the pedestal.
     * @throws std::runtime_error if the shape differs or ready() is false.
     */
    template <typename T> void push_ema(NDView<T, 2> frame) {
        if (frame.shape() != std::array<ssize_t, 2>{m_rows, m_cols}) {
            throw std::runtime_error(
                "Frame shape does not match pedestal shape");
        }

        if (!ready()) {
            throw std::runtime_error("Pedestal is not ready, cannot push");
        }

        const auto size = static_cast<std::size_t>(m_rows) * m_cols;
        const auto *data = frame.data();
        for (std::size_t index = 0; index < size; ++index) {
            push_ema_unchecked(index, data[index]);
        }
    }

    /**
     * @brief Update every pixel from a Frame using the steady-state estimator.
     * @tparam T Actual pixel type stored in frame; this is not runtime-checked.
     * @param frame Frame whose shape must exactly match the pedestal.
     * @throws std::runtime_error if the shape differs or ready() is false.
     */
    template <typename T> void push_ema(Frame &frame) {
        push_ema<T>(frame.view<T>());
    }

    /**
     * @brief Update the exponential moving average with smoothing factor
     * 1/n_samples
     * @param row Pixel row.
     * @param col Pixel column.
     * @param val New pixel value.
     * @pre row and col are valid; indices are not checked.
     * @throws std::runtime_error if ready() is false.
     */
    template <typename T>
    void push_ema(const uint32_t row, const uint32_t col, const T val) {
        if (!ready()) {
            throw std::runtime_error("Pedestal is not ready, cannot push");
        }

        push_ema_unchecked(rc_to_index(row, col), val);
    }

    /**
     * @brief Update one pixel and its cached mean without runtime checks in
     * release builds.
     * @param index Flat row-major pixel index.
     * @param value New pixel value, with weight 1 / n_samples.
     * @pre ready() is true and index is a valid flat row-major index. These
     * preconditions are asserted only in debug builds.
     */
    template <typename T>
    void push_ema_unchecked(const std::size_t index, const T value) noexcept {
        assert(m_ready);
        assert(index < static_cast<std::size_t>(m_sum.size()));

        const auto val = static_cast<double>(value);
        auto &entry = m_sum[index];
        entry.sum += val - entry.sum * m_inv_samples;
        entry.sum2 += val * val - entry.sum2 * m_inv_samples;
        m_mean[index] = static_cast<PEDESTAL_TYPE>(entry.sum * m_inv_samples);
    }

    /**
     * @brief Accumulate one initialization frame.
     * @param frame Frame whose shape must exactly match the pedestal.
     * @throws std::runtime_error if the shape differs or n_samples frames have
     * already been accumulated.
     * @note The statistics can be accessed and ready() becomes true only after
     * the n_samples frames have been added.
     */
    template <typename T> void add_init_frame(NDView<T, 2> frame) {
        if (frame.shape() != std::array<ssize_t, 2>{m_rows, m_cols}) {
            throw std::runtime_error(
                "Frame shape does not match pedestal shape");
        }

        // if the pedestal is already initialized we cannot add more frames
        if (ready()) {
            throw std::runtime_error("Pedestal initialization is already done");
        }

        for (ssize_t i = 0; i < m_sum.size(); ++i) {
            const auto val = static_cast<double>(frame[i]);
            auto &entry = m_sum[i];
            entry.sum += val;
            entry.sum2 += val * val;
        }
        m_cur_samples += 1;

        if (m_cur_samples == m_samples) {
            update_mean();
            m_ready = true;
        }
    }

    /**
     * @brief Initialize from n_samples frames after skip_first, then apply all
     * remaining file frames as steady-state updates.
     * @tparam T Pixel representation stored in the file; this is not checked.
     * @param filename Input image file. Its dimensions define the pedestal
     * shape.
     * @param n_samples Number of initialization frames.
     * @param skip_first Number of leading frames to ignore.
     * @throws std::runtime_error if fewer than n_samples frames remain after
     * skip_first, or if any constructor argument is invalid.
     */
    template <typename T>
    static FastPedestal from_file(const std::filesystem::path &filename,
                                  uint32_t n_samples = 1000,
                                  uint32_t skip_first = 0) {
        File f(filename);

        const auto total_frames = f.total_frames();
        const auto first_frame = static_cast<size_t>(skip_first);
        const auto initialization_frames = static_cast<size_t>(n_samples);
        if (first_frame > total_frames ||
            initialization_frames > total_frames - first_frame) {
            throw std::runtime_error(
                "File has less frames than the number of samples needed to "
                "initialize the pedestal");
        }

        if (skip_first > 0) {
            f.seek(static_cast<size_t>(skip_first));
        }
        const auto rows = static_cast<uint32_t>(f.rows());
        const auto cols = static_cast<uint32_t>(f.cols());
        FastPedestal pedestal(rows, cols, n_samples);
        NDArray<T, 2> frame({rows, cols});

        auto frame_index = first_frame;
        const auto initialization_end = first_frame + initialization_frames;
        while (frame_index < initialization_end) {
            f.read_into(frame.buffer());
            pedestal.template add_init_frame<T>(frame.view());
            frame_index++;
        }

        // read the rest of the file
        while (frame_index < total_frames) {
            f.read_into(frame.buffer());
            pedestal.template push_ema<T>(frame.view());
            frame_index++;
        }
        return pedestal;
    }

    /** @brief Return the number of image rows. */
    uint32_t rows() const { return m_rows; }

    /** @brief Return the number of image columns. */
    uint32_t cols() const { return m_cols; }

    /**
     * @brief Return the initialization frame count and steady-state
     * update-weight denominator.
     */
    uint32_t n_samples() const { return m_samples; }

  private:
    double variance_unchecked(ssize_t index) const {
        const auto &entry = m_sum[index];
        const auto mean = entry.sum * m_inv_samples;
        const auto variance = std::fma(-mean, mean, entry.sum2 * m_inv_samples);
        // Roundoff in the moments can make a near-zero variance negative.
        return std::max(variance, 0.0);
    }

    /**
     * @brief Write the cached mean after the final add_init_frame. All other
     * (non initialization) pushes update the cached mean immediately.
     */
    void update_mean() {
        for (ssize_t i = 0; i < m_sum.size(); i++) {
            auto &entry = m_sum[i];
            m_mean[i] = static_cast<PEDESTAL_TYPE>(entry.sum * m_inv_samples);
        }
    }
};
} // namespace aare
