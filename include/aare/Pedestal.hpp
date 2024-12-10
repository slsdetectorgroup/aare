#pragma once
#include "aare/Frame.hpp"
#include "aare/NDArray.hpp"
#include "aare/NDView.hpp"
#include <cstddef>

namespace aare {

/**
 * @brief Calculate the pedestal of a series of frames. Can be used as
 * standalone but mostly used in the ClusterFinder.
 *
 * @tparam SUM_TYPE type of the sum
 */
template <typename SUM_TYPE = double> class Pedestal {
    uint32_t m_rows;
    uint32_t m_cols;

    uint32_t m_samples;
    NDArray<uint32_t, 2> m_cur_samples;
    
    //TODO! in case of int needs to be changed to uint64_t
    NDArray<SUM_TYPE, 2> m_sum;
    NDArray<SUM_TYPE, 2> m_sum2;

  public:
    Pedestal(uint32_t rows, uint32_t cols, uint32_t n_samples = 1000)
        : m_rows(rows), m_cols(cols), m_samples(n_samples),
          m_cur_samples(NDArray<uint32_t, 2>({rows, cols}, 0)),
          m_sum(NDArray<SUM_TYPE, 2>({rows, cols})),
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

    SUM_TYPE mean(const uint32_t row, const uint32_t col) const {
        if (m_cur_samples(row, col) == 0) {
            return 0.0;
        }
        return m_sum(row, col) / m_cur_samples(row, col);
    }

    NDArray<SUM_TYPE, 2> variance() {
        NDArray<SUM_TYPE, 2> variance_array({m_rows, m_cols});
        for (uint32_t i = 0; i < m_rows * m_cols; i++) {
            variance_array(i / m_cols, i % m_cols) =
                variance(i / m_cols, i % m_cols);
        }
        return variance_array;
    }

    SUM_TYPE variance(const uint32_t row, const uint32_t col) const {
        if (m_cur_samples(row, col) == 0) {
            return 0.0;
        }
        return m_sum2(row, col) / m_cur_samples(row, col) -
               mean(row, col) * mean(row, col);
    }

    NDArray<SUM_TYPE, 2> std() {
        NDArray<SUM_TYPE, 2> standard_deviation_array({m_rows, m_cols});
        for (uint32_t i = 0; i < m_rows * m_cols; i++) {
            standard_deviation_array(i / m_cols, i % m_cols) =
                std(i / m_cols, i % m_cols);
        }

        return standard_deviation_array;
    }

    SUM_TYPE std(const uint32_t row, const uint32_t col) const {
        return std::sqrt(variance(row, col));
    }

    void clear() {
        for (uint32_t i = 0; i < m_rows * m_cols; i++) {
            clear(i / m_cols, i % m_cols);
        }
    }

    

    void clear(const uint32_t row, const uint32_t col) {
        m_sum(row, col) = 0;
        m_sum2(row, col) = 0;
        m_cur_samples(row, col) = 0;
    }
    // frame level operations
    template <typename T> void push(NDView<T, 2> frame) {
        assert(frame.size() == m_rows * m_cols);

        // TODO! move away from m_rows, m_cols
        if (frame.shape() != std::array<int64_t, 2>{m_rows, m_cols}) {
            throw std::runtime_error(
                "Frame shape does not match pedestal shape");
        }

        for (uint32_t row = 0; row < m_rows; row++) {
            for (uint32_t col = 0; col < m_cols; col++) {
                push<T>(row, col, frame(row, col));
            }
        }

        // // TODO: test the effect of #pragma omp parallel for
        // for (uint32_t index = 0; index < m_rows * m_cols; index++) {
        //     push<T>(index / m_cols, index % m_cols, frame(index));
        // }
    }
    template <typename T> void push(Frame &frame) {
        assert(frame.rows() == static_cast<size_t>(m_rows) &&
               frame.cols() == static_cast<size_t>(m_cols));
        push<T>(frame.view<T>());
    }

    // getter functions
    uint32_t rows() const { return m_rows; }
    uint32_t cols() const { return m_cols; }
    uint32_t n_samples() const { return m_samples; }
    NDArray<uint32_t, 2> cur_samples() const { return m_cur_samples; }
    NDArray<SUM_TYPE, 2> get_sum() const { return m_sum; }
    NDArray<SUM_TYPE, 2> get_sum2() const { return m_sum2; }

    // pixel level operations (should be refactored to allow users to implement
    // their own pixel level operations)
    template <typename T>
    void push(const uint32_t row, const uint32_t col, const T val_) {
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
    uint32_t index(const uint32_t row, const uint32_t col) const {
        return row * m_cols + col;
    };
};
} // namespace aare