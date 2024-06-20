#pragma once
#include "aare/core/Frame.hpp"
#include "aare/core/NDArray.hpp"
#include "aare/core/NDView.hpp"
#include <cstddef>

namespace aare {

template <typename SUM_TYPE = double> class Pedestal {
  public:
    Pedestal(int rows, int cols, int n_samples = 1000);
    ~Pedestal();

    // frame level operations
    template <typename T> void push(NDView<T, 2> frame) {
        assert(frame.size() == m_rows * m_cols);
        // TODO: test the effect of #pragma omp parallel for
        for (int index = 0; index < m_rows * m_cols; index++) {
            push<T>(index / m_cols, index % m_cols, frame(index));
        }
    }
    template <typename T> void push(Frame &frame) {
        assert(frame.rows() == static_cast<size_t>(m_rows) && frame.cols() == static_cast<size_t>(m_cols));
        push<T>(frame.view<T>());
    }
    NDArray<SUM_TYPE> mean();
    NDArray<SUM_TYPE> variance();
    NDArray<SUM_TYPE> standard_deviation();
    void clear();

    // getter functions
    inline int rows() const { return m_rows; }
    inline int cols() const { return m_cols; }
    inline int n_samples() const { return m_samples; }
    inline uint32_t *cur_samples() const { return m_cur_samples; }
    inline NDArray<SUM_TYPE, 2> get_sum() const { return m_sum; }
    inline NDArray<SUM_TYPE, 2> get_sum2() const { return m_sum2; }

    // pixel level operations (should be refactored to allow users to implement their own pixel level operations)
    template <typename T> inline void push(const int row, const int col, const T val) {
        const int idx = index(row, col);
        if (m_cur_samples[idx] < m_samples) {
            m_sum(idx) += val;
            m_sum2(idx) += val * val;
            m_cur_samples[idx]++;
        } else {
            m_sum(idx) += val - m_sum(idx) / m_cur_samples[idx];
            m_sum2(idx) += val * val - m_sum2(idx) / m_cur_samples[idx];
        }
    }
    SUM_TYPE mean(const int row, const int col) const;
    SUM_TYPE variance(const int row, const int col) const;
    SUM_TYPE standard_deviation(const int row, const int col) const;
    inline int index(const int row, const int col) const { return row * m_cols + col; };
    void clear(const int row, const int col);

  private:
    int m_rows;
    int m_cols;
    uint32_t m_samples;
    uint32_t *m_cur_samples{nullptr};
    NDArray<SUM_TYPE, 2> m_sum;
    NDArray<SUM_TYPE, 2> m_sum2;
};
} // namespace aare