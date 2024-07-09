#pragma once
#include "aare/core/Frame.hpp"
#include "aare/core/NDArray.hpp"
#include "aare/core/NDView.hpp"
#include <cstddef>

namespace aare {

template <typename SUM_TYPE = double> class Pedestal {
  public:
    Pedestal(uint32_t rows, uint32_t cols, uint32_t n_samples = 1000)
        : m_rows(rows), m_cols(cols), m_freeze(false), m_samples(n_samples),           m_cur_samples(NDArray<uint32_t, 2>({rows, cols}, 0)),m_sum(NDArray<SUM_TYPE, 2>({rows, cols})),
 m_sum2(NDArray<SUM_TYPE, 2>({rows, cols})) {
        assert(rows > 0 && cols > 0 && n_samples > 0);
        m_sum = 0;
        m_sum2 = 0;
    }
    ~Pedestal() = default;

    NDArray<SUM_TYPE, 2> mean() {
        NDArray<SUM_TYPE, 2> mean_array({m_rows, m_cols});
        for (uint32_t i = 0; i < m_rows * m_cols; i++) {
            mean_array(i / m_cols, i % m_cols) = mean(i / m_cols, i % m_cols);
        }
        return mean_array;
    }

    NDArray<SUM_TYPE, 2> variance() {
        NDArray<SUM_TYPE, 2> variance_array({m_rows, m_cols});
        for (uint32_t i = 0; i < m_rows * m_cols; i++) {
            variance_array(i / m_cols, i % m_cols) = variance(i / m_cols, i % m_cols);
        }
        return variance_array;
    }

    NDArray<SUM_TYPE, 2> standard_deviation() {
        NDArray<SUM_TYPE, 2> standard_deviation_array({m_rows, m_cols});
        for (uint32_t i = 0; i < m_rows * m_cols; i++) {
            standard_deviation_array(i / m_cols, i % m_cols) = standard_deviation(i / m_cols, i % m_cols);
        }

        return standard_deviation_array;
    }
    void clear() {
        for (uint32_t i = 0; i < m_rows * m_cols; i++) {
            clear(i / m_cols, i % m_cols);
        }
    }

    /*
     * index level operations
     */
    SUM_TYPE mean(const uint32_t row, const uint32_t col) const {
        if (m_cur_samples(row, col) == 0) {
            return 0.0;
        }
        return m_sum(row, col) / m_cur_samples(row, col);
    }
    SUM_TYPE variance(const uint32_t row, const uint32_t col) const {
        if (m_cur_samples(row, col) == 0) {
            return 0.0;
        }
        return m_sum2(row, col) / m_cur_samples(row, col) - mean(row, col) * mean(row, col);
    }
    SUM_TYPE standard_deviation(const uint32_t row, const uint32_t col) const { return std::sqrt(variance(row, col)); }

    void clear(const uint32_t row, const uint32_t col) {
        m_sum(row, col) = 0;
        m_sum2(row, col) = 0;
        m_cur_samples(row, col) = 0;
    }
    // frame level operations
    template <typename T> void push(NDView<T, 2> frame) {
        assert(frame.size() == m_rows * m_cols);
        // TODO: test the effect of #pragma omp parallel for
        for (uint32_t index = 0; index < m_rows * m_cols; index++) {
            push<T>(index / m_cols, index % m_cols, frame(index));
        }
    }
    template <typename T> void push(Frame &frame) {
        assert(frame.rows() == static_cast<size_t>(m_rows) && frame.cols() == static_cast<size_t>(m_cols));
        push<T>(frame.view<T>());
    }

    // getter functions
    inline uint32_t rows() const { return m_rows; }
    inline uint32_t cols() const { return m_cols; }
    inline uint32_t n_samples() const { return m_samples; }
    inline NDArray<uint32_t, 2> cur_samples() const { return m_cur_samples; }
    inline NDArray<SUM_TYPE, 2> get_sum() const { return m_sum; }
    inline NDArray<SUM_TYPE, 2> get_sum2() const { return m_sum2; }

    // pixel level operations (should be refactored to allow users to implement their own pixel level operations)
    template <typename T> inline void push(const uint32_t row, const uint32_t col, const T val_) {
        if (m_freeze) {
            return;
        }
        SUM_TYPE val = static_cast<SUM_TYPE>(val_);
        const uint32_t idx = index(row, col);
        if (m_cur_samples(idx) < m_samples) {
            m_sum(idx) += val;
            m_sum2(idx) += val * val;
            m_cur_samples(idx)++;
        } else {
            m_sum(idx) += val - m_sum(idx) / m_cur_samples(idx);
            m_sum2(idx) += val * val - m_sum2(idx) / m_cur_samples(idx);
        }
    }
    inline uint32_t index(const uint32_t row, const uint32_t col) const { return row * m_cols + col; };
    void set_freeze(bool freeze) { m_freeze = freeze; }

  private:
    uint32_t m_rows;
    uint32_t m_cols;
    bool m_freeze;
    uint32_t m_samples;
    NDArray<uint32_t, 2> m_cur_samples;
    NDArray<SUM_TYPE, 2> m_sum;
    NDArray<SUM_TYPE, 2> m_sum2;
};
} // namespace aare