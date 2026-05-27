#pragma once
/*
Implmenetation of a basic pixel histogram class with templated axis and storage type.
row, col are integers and the per pixel histogram axis is AxisType.

Storage layout matches the existing PixelHistogram/PedestalTrackingPixelHistogram
hdata() shape: NDArray<StorageType, 3> indexed as (row, col, bin) in row-major
order, i.e. bin is the fastest-varying dimension. This keeps downstream
consumers (memcpy stitching across thread shards, numpy reshaping in the
python bindings) unchanged.

Bin policy: regular binning on [xmin, xmax). Values outside this range are
silently dropped (matches boost::histogram axis option::none_t used by the
boost-backed classes).
*/

#include "aare/NDArray.hpp"
#include "aare/NDView.hpp"

#include <cstddef>
#include <stdexcept>

namespace aare {

template <typename T, typename StorageType> class PixelHistogramImpl {
    NDArray<StorageType, 3> m_hdata;
    NDArray<T, 1> m_edges;
    int m_rows;
    int m_cols;
    int m_n_bins;
    T m_xmin;
    T m_xmax;
    // n_bins / (xmax - xmin), precomputed to keep the hot path
    // division-free.
    T m_scale;

  public:
    PixelHistogramImpl(int rows, int cols, int n_bins, T xmin, T xmax);

    void fill(const NDView<T, 2> &frame);
    void fill(int row, int col, T value);

    NDArray<StorageType, 3> hdata() const;
    // Zero-copy view of the underlying [rows x cols x n_bins] storage.
    // Lifetime is tied to *this. Use for low-level merge/stitching paths;
    // prefer hdata() for the public API where you want an owned copy.
    NDView<StorageType, 3> view() const;
    NDArray<T, 1> bin_centers() const;
    NDArray<T, 1> bin_edges() const;
};

template <typename T, typename StorageType>
PixelHistogramImpl<T, StorageType>::PixelHistogramImpl(int rows, int cols,
                                                       int n_bins, T xmin,
                                                       T xmax)
    : m_hdata(NDArray<StorageType, 3>({static_cast<ssize_t>(rows),
                                       static_cast<ssize_t>(cols),
                                       static_cast<ssize_t>(n_bins)},
                                      StorageType{0})),
      m_edges(NDArray<T, 1>({static_cast<ssize_t>(n_bins + 1)})), m_rows(rows),
      m_cols(cols), m_n_bins(n_bins), m_xmin(xmin), m_xmax(xmax),
      m_scale(static_cast<T>(n_bins) / (xmax - xmin)) {
    if (rows < 1 || cols < 1 || n_bins < 1) {
        throw std::invalid_argument(
            "PixelHistogramImpl requires positive rows, cols and bins");
    }
    if (!(xmax > xmin)) {
        throw std::invalid_argument("PixelHistogramImpl requires xmax > xmin");
    }

    const T range = xmax - xmin;
    for (int i = 0; i <= n_bins; ++i) {
        m_edges(i) =
            xmin + (static_cast<T>(i) * range) / static_cast<T>(n_bins);
    }
}

template <typename T, typename StorageType>
void PixelHistogramImpl<T, StorageType>::fill(const NDView<T, 2> &frame) {
    if (frame.shape(0) != m_rows || frame.shape(1) != m_cols) {
        throw std::invalid_argument(
            "PixelHistogramImpl::fill: frame shape does not match histogram");
    }
    for (int row = 0; row < m_rows; ++row) {
        for (int col = 0; col < m_cols; ++col) {
            const T val = frame(row, col);
            if (val < m_xmin || val >= m_xmax) {
                continue;
            }
            int bin = static_cast<int>((val - m_xmin) * m_scale);
            // Guard against floating-point rounding pushing val just below
            // xmax to bin == n_bins.
            if (bin >= m_n_bins) {
                bin = m_n_bins - 1;
            }
            ++m_hdata(row, col, bin);
        }
    }
}

template <typename T, typename StorageType>
void PixelHistogramImpl<T, StorageType>::fill(int row, int col, T value) {
    if (value < m_xmin || value >= m_xmax) {
        return;
    }
    int bin = static_cast<int>((value - m_xmin) * m_scale);
    if (bin >= m_n_bins) {
        bin = m_n_bins - 1;
    }
    ++m_hdata(row, col, bin);
}

template <typename T, typename StorageType>
NDArray<StorageType, 3> PixelHistogramImpl<T, StorageType>::hdata() const {
    return m_hdata;
}

template <typename T, typename StorageType>
NDView<StorageType, 3> PixelHistogramImpl<T, StorageType>::view() const {
    return m_hdata.view();
}

template <typename T, typename StorageType>
NDArray<T, 1> PixelHistogramImpl<T, StorageType>::bin_centers() const {
    NDArray<T, 1> centers({static_cast<ssize_t>(m_n_bins)});
    for (int i = 0; i < m_n_bins; ++i) {
        centers(i) = (m_edges(i) + m_edges(i + 1)) / T(2);
    }
    return centers;
}

template <typename T, typename StorageType>
NDArray<T, 1> PixelHistogramImpl<T, StorageType>::bin_edges() const {
    return m_edges;
}

} // namespace aare
