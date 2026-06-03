#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "aare/hist/PixelHistogramImpl.hpp"

TEST_CASE("PixelHistogramImpl construct a small histogram and fill with a few "
          "values") {
    int rows = 3;
    int cols = 5;
    int n_bins = 10;
    double xmin = 0.0;
    double xmax = 1.0;
    aare::PixelHistogramImpl<double, int64_t> hist(rows, cols, n_bins, xmin,
                                                   xmax);

    // Check that the histogram is initialized correctly
    REQUIRE(hist.view().shape(0) == rows);
    REQUIRE(hist.view().shape(1) == cols);
    REQUIRE(hist.view().shape(2) == n_bins);

    auto v = hist.view();
    for (int row = 0; row < rows; ++row) {
        for (int col = 0; col < cols; ++col) {
            for (int bin = 0; bin < n_bins; ++bin) {
                REQUIRE(v(row, col, bin) == 0);
            }
        }
    }

    auto edges = hist.bin_edges();
    for (int i = 0; i <= n_bins; ++i) {
        REQUIRE_THAT(edges(i), Catch::Matchers::WithinRel(0.1 * i, 1e-12));
    }

    hist.fill(1, 1, 0.05);
    REQUIRE(v(1, 1, 0) == 1);

    hist.fill(1, 1, 0.0999);
    REQUIRE(v(1, 1, 0) == 2);

    hist.fill(1, 1, 0.1);
    REQUIRE(
        v(1, 1, 1) ==
        1); // At the edge should go to the bin with the edge as the lower bound
}
