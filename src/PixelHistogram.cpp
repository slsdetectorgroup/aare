#include "aare/PixelHistogram.hpp"

#include <boost/histogram.hpp>
#include <boost/histogram/storage_adaptor.hpp>
#include <boost/histogram/unsafe_access.hpp>
#include <cstring>

namespace aare {


PixelHistogram::PixelHistogram(int rows, int cols, int n_bins, double xmin, double xmax): hist_(
              bh::axis::regular<double, bh::use_default, bh::use_default,
                                 bh::axis::option::none_t>(n_bins, xmin, xmax, "value"),
              bh::axis::integer<int, bh::use_default, bh::axis::option::none_t>(0, cols, "y"),
              bh::axis::integer<int, bh::use_default, bh::axis::option::none_t>(0, rows, "x")
          ) {
    // Constructor implementation
}

NDArray<int32_t,3> PixelHistogram::hdata() const {
    const auto bins = static_cast<ssize_t>(hist_.axis(0).size());
    const auto cols = static_cast<ssize_t>(hist_.axis(1).size());
    const auto rows = static_cast<ssize_t>(hist_.axis(2).size());

    NDArray<int32_t, 3> data({rows, cols, bins});

    const auto &storage = bh::unsafe_access::storage(hist_);
    std::memcpy(data.data(), storage.data(), data.total_bytes());

    return data;
}

void PixelHistogram::fill(const NDView<double, 2> &image) {
    for (ssize_t row = 0; row < image.shape(0); ++row) {
        for (ssize_t col = 0; col < image.shape(1); ++col) {
            hist_(image(row, col), col, row);
        }
    }
}

NDArray<double, 1> PixelHistogram::bin_centers() const {
    const auto& value_axis = hist_.axis(0);
    const auto n_bins = static_cast<ssize_t>(value_axis.size());
    
    NDArray<double, 1> centers({n_bins});
    
    for (ssize_t i = 0; i < n_bins; ++i) {
        // Get the left and right edges of the bin and compute the center
        double left = value_axis.value(i);
        double right = value_axis.value(i + 1);
        centers(i) = (left + right) / 2.0;
    }
    
    return centers;
}

NDArray<double, 1> PixelHistogram::bin_edges() const {
    const auto& value_axis = hist_.axis(0);
    const auto n_bins = static_cast<ssize_t>(value_axis.size());

    NDArray<double, 1> edges({n_bins + 1});

    for (ssize_t i = 0; i <= n_bins; ++i) {
        edges(i) = value_axis.value(i);
    }

    return edges;
}

} // namespace aare
