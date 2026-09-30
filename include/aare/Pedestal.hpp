// SPDX-License-Identifier: MPL-2.0
#pragma once
#include "aare/Frame.hpp"
#include "aare/NDArray.hpp"
#include "aare/NDView.hpp"
#include <algorithm>
#include <cstddef>

namespace aare {

/**
 * @brief Maintain per-pixel mean and population standard deviation.
 *
 * Each pixel accumulates its first n_samples values. Subsequent pushes update
 * the exponential moving average with a smoothing factor of 1 / n_samples.
 * Statistics are available during initialization and are zero for empty pixels.
 * Internal moments and the private variance intermediate use double precision.
 * @tparam SUM_TYPE Type returned for the mean and standard deviation.
 */
template <typename SUM_TYPE = double> class Pedestal {
    uint32_t m_rows;
    uint32_t m_cols;

    uint32_t m_samples;
    NDArray<uint32_t, 2> m_cur_samples;

    NDArray<double, 2> m_sum;
    NDArray<double, 2> m_sum2;

    // Cache mean since it is used over and over in the ClusterFinder
    // This optimization is related to the access pattern of the ClusterFinder
    // Relies on having more reads than pushes to the pedestal
    NDArray<SUM_TYPE, 2> m_mean;

  public:
    /**
     * @brief Construct an empty pedestal with zero mean and standard deviation.
     * @param rows Number of image rows.
     * @param cols Number of image columns.
     * @param n_samples Number of initialization samples per pixel and
     * reciprocal of the weight assigned to each subsequent value.
     * @throws std::runtime_error if rows, cols, or n_samples is zero.
     */
    Pedestal(uint32_t rows, uint32_t cols, uint32_t n_samples = 1000)
        : m_rows(rows), m_cols(cols), m_samples(n_samples),
          m_cur_samples(NDArray<uint32_t, 2>({rows, cols}, 0)),
          m_sum(NDArray<double, 2>({rows, cols}, 0.0)),
          m_sum2(NDArray<double, 2>({rows, cols}, 0.0)),
          m_mean(NDArray<SUM_TYPE, 2>({rows, cols}, SUM_TYPE(0))) {
        if (!(rows > 0 && cols > 0 && n_samples > 0)) {
            throw std::runtime_error(
                fmt::format("Invalid parameters for Pedestal: rows={}, "
                            "cols={}, n_samples={} need to be positive",
                            rows, cols, n_samples));
        }
    }

    ~Pedestal() = default;

    /**
     * @brief Return a non-owning view of the cached mean.
     * @note The caller must treat the data as read-only and must not retain the
     * view after this object is destroyed, moved, or assigned.
     */
    NDView<const SUM_TYPE, 2> view() const { return m_mean.view(); }

    /**
     * @brief Return the cached mean at (row, col), or zero for an empty pixel.
     * @pre row and col are valid pixel indices.
     */
    SUM_TYPE mean(const uint32_t row, const uint32_t col) const {
        return m_mean(row, col);
    }

    /** @brief Return a copy of the cached mean. */
    NDArray<SUM_TYPE, 2> mean() const { return m_mean; }

    /**
     * @brief Calculate the population standard deviation at (row, col).
     * @pre row and col are valid pixel indices.
     * @note Variance is normalized by the pixel's current sample count. Empty
     * pixels return zero, and negative variance from roundoff is clamped to
     * zero.
     */
    SUM_TYPE std(const uint32_t row, const uint32_t col) const {
        return std::sqrt(variance(row, col));
    }

    /**
     * @brief Calculate and return the population standard deviation of every
     * pixel. Empty pixels return zero.
     */
    NDArray<SUM_TYPE, 2> std() const {
        NDArray<SUM_TYPE, 2> res({m_rows, m_cols});
        for (uint32_t row = 0; row < m_rows; ++row) {
            for (uint32_t col = 0; col < m_cols; ++col) {
                res(row, col) = std(row, col);
            }
        }
        return res;
    }

    /**
     * @brief Zero the moments, cached mean, and sample counts of every pixel.
     */
    void clear() {
        m_sum = 0;
        m_sum2 = 0;
        m_cur_samples = 0;
        m_mean = 0;
    }

    /**
     * @brief Zero the moments, cached mean, and sample count at (row, col).
     * @pre row and col are valid pixel indices.
     */
    void clear(const uint32_t row, const uint32_t col) {
        m_sum(row, col) = 0;
        m_sum2(row, col) = 0;
        m_cur_samples(row, col) = 0;
        m_mean(row, col) = 0;
    }

    /**
     * @brief Accumulate or exponentially update every pixel and its cached
     * mean.
     * @param frame Frame whose shape must exactly match the pedestal.
     * @throws std::runtime_error if the shape differs.
     */
    template <typename T> void push(NDView<T, 2> frame) {
        // TODO! move away from m_rows, m_cols
        if (frame.shape() != std::array<ssize_t, 2>{m_rows, m_cols}) {
            throw std::runtime_error(
                "Frame shape does not match pedestal shape");
        }

        for (size_t row = 0; row < m_rows; row++) {
            for (size_t col = 0; col < m_cols; col++) {
                push<T>(row, col, frame(row, col));
            }
        }
    }

    /**
     * @brief Push only pixels whose absolute difference from the cached mean is
     * strictly less than their threshold.
     * @param frame Frame whose shape must exactly match the pedestal.
     * @param threshold Per-pixel thresholds with the same shape as the
     * pedestal.
     * @throws std::runtime_error if either shape differs.
     * @note Rejected pixels keep their statistics and sample counts unchanged.
     */
    template <typename T>
    void push_with_threshold(const NDView<T, 2> frame,
                             const NDView<SUM_TYPE, 2> threshold) {
        // TODO! move away from m_rows, m_cols
        if (frame.shape() != std::array<ssize_t, 2>{m_rows, m_cols}) {
            throw std::runtime_error(
                "Frame shape does not match pedestal shape");
        }

        if (threshold.shape() != std::array<ssize_t, 2>{m_rows, m_cols}) {
            throw std::runtime_error(
                "Threshold shape does not match pedestal shape");
        }

        for (size_t row = 0; row < m_rows; row++) {
            for (size_t col = 0; col < m_cols; col++) {
                if (fabs(frame(row, col) - mean(row, col)) <
                    threshold(row, col)) {
                    push<T>(row, col, frame(row, col));
                }
            }
        }
    }

    /**
     * @brief Accumulate or exponentially update every pixel from a Frame.
     * @tparam T Actual pixel type stored in frame; this is not runtime-checked.
     * @param frame Frame whose shape must exactly match the pedestal.
     * @throws std::runtime_error if the shape differs.
     */
    template <typename T> void push(Frame &frame) { push<T>(frame.view<T>()); }

    /** @brief Return the number of image rows. */
    uint32_t rows() const { return m_rows; }

    /** @brief Return the number of image columns. */
    uint32_t cols() const { return m_cols; }

    /**
     * @brief Return the initialization sample count per pixel and steady-state
     * update-weight denominator.
     */
    uint32_t n_samples() const { return m_samples; }

    /**
     * @brief Return a copy of the per-pixel initialization sample counts.
     * @note Each count is in [0, n_samples] and does not change during
     * steady-state pushes. Thresholded pushes can leave counts unequal.
     */
    NDArray<uint32_t, 2> cur_samples() const { return m_cur_samples; }

    /**
     * @brief Accumulate or exponentially update one pixel and its cached mean.
     * @param row Pixel row.
     * @param col Pixel column.
     * @param val_ New pixel value, with weight 1 / n_samples after
     * initialization.
     * @pre row and col are valid pixel indices.
     */
    template <typename T>
    void push(const uint32_t row, const uint32_t col, const T val_) {
        const auto val = static_cast<double>(val_);
        if (m_cur_samples(row, col) < m_samples) {
            m_sum(row, col) += val;
            m_sum2(row, col) += val * val;
            m_cur_samples(row, col)++;
        } else {
            m_sum(row, col) += val - m_sum(row, col) / m_samples;
            m_sum2(row, col) += val * val - m_sum2(row, col) / m_samples;
        }
        // Since we just did a push we know that m_cur_samples(row, col) is at
        // least 1
        m_mean(row, col) = m_sum(row, col) / m_cur_samples(row, col);
    }

  private:
    double variance(const uint32_t row, const uint32_t col) const {
        if (m_cur_samples(row, col) == 0) {
            return 0.0;
        }
        const auto mean = m_sum(row, col) / m_cur_samples(row, col);
        const auto var =
            m_sum2(row, col) / m_cur_samples(row, col) - mean * mean;
        // Roundoff in the moments can make a near-zero variance negative.
        return std::max(var, 0.0);
    }
};
} // namespace aare
