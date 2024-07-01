#include "aare/processing/Pedestal.hpp"
#include <cmath>
#include <cstddef>

namespace aare {
template <typename SUM_TYPE>
Pedestal<SUM_TYPE>::Pedestal(int rows, int cols, int n_samples)
    : m_freeze(false), m_rows(rows), m_cols(cols), m_samples(n_samples), m_sum(NDArray<SUM_TYPE, 2>({rows, cols})),
      m_sum2(NDArray<SUM_TYPE, 2>({rows, cols})), m_cur_samples(new uint32_t[static_cast<uint64_t>(rows) * cols]{}) {
    assert(rows > 0 && cols > 0 && n_samples > 0);
    m_sum = 0;
    m_sum2 = 0;
}

template <typename SUM_TYPE> NDArray<SUM_TYPE, 2> Pedestal<SUM_TYPE>::mean() {
    NDArray<SUM_TYPE, 2> mean_array({m_rows, m_cols});
    for (int i = 0; i < m_rows * m_cols; i++) {
        mean_array(i / m_cols, i % m_cols) = mean(i / m_cols, i % m_cols);
    }
    return mean_array;
}

template <typename SUM_TYPE> NDArray<SUM_TYPE, 2> Pedestal<SUM_TYPE>::variance() {
    NDArray<SUM_TYPE, 2> variance_array({m_rows, m_cols});
    for (int i = 0; i < m_rows * m_cols; i++) {
        variance_array(i / m_cols, i % m_cols) = variance(i / m_cols, i % m_cols);
    }
    return variance_array;
}

template <typename SUM_TYPE> NDArray<SUM_TYPE, 2> Pedestal<SUM_TYPE>::standard_deviation() {
    NDArray<SUM_TYPE, 2> standard_deviation_array({m_rows, m_cols});
    for (int i = 0; i < m_rows * m_cols; i++) {
        standard_deviation_array(i / m_cols, i % m_cols) = standard_deviation(i / m_cols, i % m_cols);
    }

    return standard_deviation_array;
}
template <typename SUM_TYPE> void Pedestal<SUM_TYPE>::clear() {
    for (int i = 0; i < m_rows * m_cols; i++) {
        clear(i / m_cols, i % m_cols);
    }
}

/*
 * index level operations
 */
template <typename SUM_TYPE> SUM_TYPE Pedestal<SUM_TYPE>::mean(const int row, const int col) const {
    if (m_cur_samples[index(row, col)] == 0) {
        return 0.0;
    }
    return m_sum(row, col) / m_cur_samples[index(row, col)];
}
template <typename SUM_TYPE> SUM_TYPE Pedestal<SUM_TYPE>::variance(const int row, const int col) const {
    if (m_cur_samples[index(row, col)] == 0) {
        return 0.0;
    }
    return m_sum2(row, col) / m_cur_samples[index(row, col)] - mean(row, col) * mean(row, col);
}
template <typename SUM_TYPE> SUM_TYPE Pedestal<SUM_TYPE>::standard_deviation(const int row, const int col) const {
    return std::sqrt(variance(row, col));
}

template <typename SUM_TYPE> void Pedestal<SUM_TYPE>::clear(const int row, const int col) {
    m_sum(row, col) = 0;
    m_sum2(row, col) = 0;
    m_cur_samples[index(row, col)] = 0;
}

template <typename SUM_TYPE> Pedestal<SUM_TYPE>::~Pedestal() { delete[] m_cur_samples; }

template class Pedestal<double>;
template class Pedestal<float>;
template class Pedestal<long double>;

} // namespace aare