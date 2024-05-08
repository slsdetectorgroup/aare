#include "aare/processing/Pedestal.hpp"
#include <cmath>
#include <cstddef>

namespace aare {
Pedestal::Pedestal(int rows, int cols, int n_samples)
    : m_rows(rows), m_cols(cols), m_samples(n_samples), m_sum(new double[rows * cols]{}),
      m_sum2(new double[rows * cols]{}), m_cur_samples(new uint32_t[rows * cols]{}) {
    assert(rows > 0 && cols > 0 && n_samples > 0);
}

NDArray<double, 2> Pedestal::mean() {
    NDArray<double, 2> mean_array({m_rows, m_cols});
    for (int i = 0; i < m_rows * m_cols; i++) {
        mean_array(i % m_cols, i / m_cols) = mean(i % m_cols, i / m_cols);
    }
    return mean_array;
}

NDArray<double, 2> Pedestal::variance() {
    NDArray<double, 2> variance_array({m_rows, m_cols});
    for (int i = 0; i < m_rows * m_cols; i++) {
        variance_array(i % m_cols, i / m_cols) = variance(i % m_cols, i / m_cols);
    }
    return variance_array;
}

NDArray<double, 2> Pedestal::standard_deviation() {
    NDArray<double, 2> standard_deviation_array({m_rows, m_cols});
    for (int i = 0; i < m_rows * m_cols; i++) {
        standard_deviation_array(i % m_cols, i / m_cols) = standard_deviation(i % m_cols, i / m_cols);
    }
    return standard_deviation_array;
}
void Pedestal::clear() {
    for (int i = 0; i < m_rows * m_cols; i++) {
        clear(i % m_cols, i / m_cols);
    }
}

/*
 * index level operations
 */
double Pedestal::mean(const int row, const int col) const {
    if (m_cur_samples[index(row, col)] == 0) {
        return 0.0;
    }
    return m_sum[index(row, col)] / m_cur_samples[index(row, col)];
}
double Pedestal::variance(const int row, const int col) const {
    if (m_cur_samples[index(row, col)] == 0) {
        return 0.0;
    }
    return m_sum2[index(row, col)] / m_cur_samples[index(row, col)] - mean(row, col) * mean(row, col);
}
double Pedestal::standard_deviation(const int row, const int col) const { return std::sqrt(variance(row, col)); }

void Pedestal::clear(const int row, const int col) {
    m_sum[index(row, col)] = 0;
    m_sum2[index(row, col)] = 0;
    m_cur_samples[index(row, col)] = 0;
}

Pedestal::~Pedestal() {
    delete[] m_sum;
    delete[] m_sum2;
    delete[] m_cur_samples;
}

} // namespace aare