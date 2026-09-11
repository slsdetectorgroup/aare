// SPDX-License-Identifier: MPL-2.0
#pragma once
#include "aare/Frame.hpp"
#include "aare/NDArray.hpp"
#include "aare/NDView.hpp"
#include <algorithm>
#include <cstddef>

namespace aare {

/**
 * @brief Calculate the pedestal of a series of frames. Can be used as
 * standalone but mostly used in the ClusterFinder.
 *
 * Internal sums and sums of squares are stored in double precision.
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

    NDArray<SUM_TYPE, 2> mean() { return m_mean; }

    NDView<const SUM_TYPE, 2> view() const { return m_mean.view(); }

    SUM_TYPE mean(const uint32_t row, const uint32_t col) const {
        return m_mean(row, col);
    }

    SUM_TYPE std(const uint32_t row, const uint32_t col) const {
        return std::sqrt(variance(row, col));
    }

    NDArray<SUM_TYPE, 2> std() {
        NDArray<SUM_TYPE, 2> standard_deviation_array({m_rows, m_cols});
        for (uint32_t i = 0; i < m_rows * m_cols; i++) {
            standard_deviation_array(i / m_cols, i % m_cols) =
                std(i / m_cols, i % m_cols);
        }

        return standard_deviation_array;
    }

    void clear() {
        m_sum = 0;
        m_sum2 = 0;
        m_cur_samples = 0;
        m_mean = 0;
    }

    void clear(const uint32_t row, const uint32_t col) {
        m_sum(row, col) = 0;
        m_sum2(row, col) = 0;
        m_cur_samples(row, col) = 0;
        m_mean(row, col) = 0;
    }

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

    template <typename T> void push(Frame &frame) { push<T>(frame.view<T>()); }

    // getter functions
    uint32_t rows() const { return m_rows; }
    uint32_t cols() const { return m_cols; }
    uint32_t n_samples() const { return m_samples; }
    NDArray<uint32_t, 2> cur_samples() const { return m_cur_samples; }

    // pixel level operations (should be refactored to allow users to implement
    // their own pixel level operations)
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
