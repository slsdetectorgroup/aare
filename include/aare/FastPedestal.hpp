// SPDX-License-Identifier: MPL-2.0
#pragma once
#include "aare/File.hpp"
#include "aare/Frame.hpp"
#include "aare/NDArray.hpp"
#include "aare/NDView.hpp"
#include <cstddef>
#include <sys/types.h>

namespace aare {

/**
 * @brief Calculate the pedestal of a series of frames. Can be used as
 * standalone but mostly used in the ClusterFinder. Internal calculations are
 * performed using double precision.
 *
 * @tparam PEDESTAL_TYPE type of the exposed mean and std
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

    ~FastPedestal() = default;

    /**
     * @brief Return a non-owning view of the cached mean.
     * @note The caller must treat the data as read-only and must not retain the
     * view after this object is destroyed, moved, or assigned.
     */
    const NDView<PEDESTAL_TYPE, 2> view() const { return m_mean.view(); }

    /**
     * @brief Return a copy of the cached mean.
     * @note The result is meaningful after ready() is true.
     */
    NDArray<PEDESTAL_TYPE, 2> mean() { return m_mean; }

    /**
     * @brief Return the cached mean at (row, col).
     * @pre ready() is true, the cache is current, and both indices are valid;
     * indices are not checked.
     */
    PEDESTAL_TYPE mean(uint32_t row, uint32_t col) const {
        return m_mean(row, col);
    }

    /**
     * @brief Return the cached mean at a flat row-major index.
     * @pre ready() is true, the cache is current, and index is valid; the index
     * is not checked.
     */
    PEDESTAL_TYPE mean(ssize_t index) const { return m_mean[index]; }

    /**
     * @brief Calculate and return the variance of every pixel.
     * @note Results use n_samples as the normalization and are meaningful only
     * after ready() is true.
     */
    NDArray<PEDESTAL_TYPE, 2> variance() {
        NDArray<PEDESTAL_TYPE, 2> res({m_rows, m_cols});
        for (ssize_t i = 0; i < m_sum.size(); ++i) {
            res[i] = variance(i);
        }
        return res;
    }

    /**
     * @brief Calculate the variance at (row, col).
     * @pre ready() is true and both indices are valid; indices are not checked.
     */
    PEDESTAL_TYPE variance(const uint32_t row, const uint32_t col) const {
        return variance(rc_to_index(row, col));
    }

    /**
     * @brief Calculate the variance at a flat row-major index.
     * @pre ready() is true and index is valid; the index is not checked.
     */
    PEDESTAL_TYPE variance(ssize_t index) const {
        const auto &entry = m_sum[index];
        const auto m = entry.sum * m_inv_samples;
        return std::fma(-m, m, entry.sum2 * m_inv_samples);
    }

    /**
     * @brief Calculate and return the standard deviation of every pixel.
     * @note Results use n_samples as the normalization and are meaningful only
     * after ready() is true.
     */
    NDArray<PEDESTAL_TYPE, 2> std() {
        NDArray<PEDESTAL_TYPE, 2> res({m_rows, m_cols});
        for (ssize_t i = 0; i < m_sum.size(); ++i) {
            res[i] = std(i);
        }
        return res;
    }

    /**
     * @brief Calculate the standard deviation at (row, col).
     * @pre ready() is true and both indices are valid; indices are not checked.
     */
    PEDESTAL_TYPE std(const uint32_t row, const uint32_t col) const {
        return std::sqrt(variance(row, col));
    }

    /**
     * @brief Calculate the standard deviation at a flat row-major index.
     * @pre ready() is true and index is valid; the index is not checked.
     */
    PEDESTAL_TYPE std(ssize_t index) const {
        return std::sqrt(variance(index));
    }

    /** @brief Return whether n_samples initialization frames were accumulated.
     */
    bool ready() const { return m_ready; }

    /**
     * @brief Return the number of initialization frames accumulated up to the
     * steady state value.
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
     * @brief Update every pixel using the steady-state exponential estimator.
     * @param frame Frame whose shape must exactly match the pedestal.
     * @throws std::runtime_error if the shape differs or ready() is false.
     */
    template <typename T> void push(NDView<T, 2> frame) {
        if (frame.shape() != std::array<ssize_t, 2>{m_rows, m_cols}) {
            throw std::runtime_error(
                "Frame shape does not match pedestal shape");
        }

        if (!ready()) {
            throw std::runtime_error("Pedestal is not ready, cannot push");
        }

        // TODO! update with push_fast
        for (size_t row = 0; row < m_rows; row++) {
            for (size_t col = 0; col < m_cols; col++) {
                push<T>(row, col, frame(row, col));
            }
        }
    }

    /**
     * @brief Update every pixel from a Frame using the steady-state estimator.
     * @tparam T Actual pixel type stored in frame; this is not runtime-checked.
     * @throws std::runtime_error if the shape differs or ready() is false.
     */
    template <typename T> void push(Frame &frame) { push<T>(frame.view<T>()); }

    /**
     * @brief Update one pixel and its cached mean using the steady-state
     * estimator.
     * @pre row and col are valid; indices are not checked.
     * @throws std::runtime_error if ready() is false.
     */
    template <typename T>
    void push(const uint32_t row, const uint32_t col, const T val) {
        if (!ready()) {
            throw std::runtime_error("Pedestal is not ready, cannot push");
        }

        push_fast(rc_to_index(row, col), val);
    }

    /**
     * @brief Update one pixel and its cached mean without runtime checks in
     * release builds.
     * @pre ready() is true and index is a valid flat row-major index. These
     * preconditions are asserted only in debug builds.
     */
    template <typename T>
    void push_fast(const std::size_t index, const T value) noexcept {
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
     * @note The cached mean is updated and ready() becomes true only when the
     * n_samples-th frame is pushed.
     */
    template <typename T> void push_init(NDView<T, 2> frame) {
        if (frame.shape() != std::array<ssize_t, 2>{m_rows, m_cols}) {
            throw std::runtime_error(
                "Frame shape does not match pedestal shape");
        }

        // if already full, throw an error
        if (m_cur_samples == m_samples) {
            throw std::runtime_error("Pedestal is full");
        }

        for (size_t row = 0; row < m_rows; row++) {
            for (size_t col = 0; col < m_cols; col++) {
                const auto val = static_cast<double>(frame(row, col));
                auto &entry = m_sum(row, col);
                entry.sum += val;
                entry.sum2 += val * val;
            }
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
     * @pre skip_first + n_samples does not exceed the file's frame count.
     * @throws std::runtime_error if too few initialization frames are
     * available.
     */
    template <typename T>
    static FastPedestal from_file(const std::filesystem::path &filename,
                                  uint32_t n_samples = 1000,
                                  uint32_t skip_first = 0) {
        File f(filename);

        if ((f.total_frames() - skip_first) < n_samples) {
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

        uint32_t frame_index = skip_first;
        while (frame_index < skip_first + n_samples) {
            f.read_into(frame.buffer());
            pedestal.template push_init<T>(frame.view());
            frame_index++;
        }

        // read the rest of the file
        while (frame_index < f.total_frames()) {
            f.read_into(frame.buffer());
            pedestal.template push<T>(frame.view());
            frame_index++;
        }
        return pedestal;
    }

    /** @brief Return the number of image rows. */
    uint32_t rows() const { return m_rows; }

    /** @brief Return the number of image columns. */
    uint32_t cols() const { return m_cols; }

    /**
     * @brief Return the initialization count and steady-state time constant.
     */
    uint32_t n_samples() const { return m_samples; }

  private:
    /**
     * @brief Write the cached mean after the final push_init. All other
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
