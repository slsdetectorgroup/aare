#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "aare/NDArray.hpp"
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

TEST_CASE("Fill a small histogram from an NDArray") {
    int rows = 2;
    int cols = 3;
    int n_bins = 10;
    double xmin = 0.0;
    double xmax = 1.0;
    aare::PixelHistogramImpl<double, int64_t> hist(rows, cols, n_bins, xmin,
                                                   xmax);

    aare::NDArray<double> frame({rows, cols});
    frame(0, 0) = 0.05; // bin 0
    frame(0, 1) = 0.15; // bin 1
    frame(0, 2) = 0.25; // bin 2
    frame(1, 0) = 0.35; // bin 3
    frame(1, 1) = 0.45; // bin 4
    frame(1, 2) = 1.5;  // out of range

    hist.fill(frame.view());

    auto v = hist.view();
    REQUIRE(v(0, 0, 0) == 1);
    REQUIRE(v(0, 1, 1) == 1);
    REQUIRE(v(0, 2, 2) == 1);
    REQUIRE(v(1, 0, 3) == 1);
    REQUIRE(v(1, 1, 4) == 1);
}

TEST_CASE("Check that pixel histogram does not overflow"){
    int rows = 1;
    int cols = 1;
    int n_bins = 10;
    double xmin = 0.0;
    double xmax = 1.0;
    aare::PixelHistogramImpl<double, uint8_t> hist_u8(rows, cols, n_bins, xmin,
                                                   xmax);

    for (int i = 0; i < 255; ++i) {
        hist_u8.fill(0, 0, 0.05);
    }

    auto v0 = hist_u8.view();
    REQUIRE(v0(0, 0, 0) == 255);

    hist_u8.fill(0, 0, 0.05);
    REQUIRE(v0(0, 0, 0) == 255);

    aare::PixelHistogramImpl<double, int8_t> hist_i8(rows, cols, n_bins, xmin,
                                                   xmax);

    for (int i = 0; i < 350; ++i) {
        hist_i8.fill(0, 0, 0.05);
    }

    auto v1 = hist_i8.view();
    REQUIRE(v1(0, 0, 0) == 127);

}

TEST_CASE("Check that values outside the range do not affect the histogram") {
    int rows = 1;
    int cols = 1;
    int n_bins = 10;
    double xmin = 0.0;
    double xmax = 1.0;
    aare::PixelHistogramImpl<double, double> hist(rows, cols, n_bins, xmin,
                                                   xmax);
    hist.fill(0, 0, -0.1); // below range
    hist.fill(0, 0, 1.0);  // at upper edge (should be out of range)
    hist.fill(0, 0, 1.1);  // above range
   
    auto v = hist.view();
    auto total = std::accumulate(v.begin(), v.end(), 0);
    REQUIRE(total == 0);

}

TEST_CASE("Check that row and column bounds are checked") {
    int rows = 1;
    int cols = 1;
    int n_bins = 10;
    double xmin = 0.0;
    double xmax = 1.0;
    aare::PixelHistogramImpl<double, double> hist(rows, cols, n_bins, xmin,
                                                   xmax);
    REQUIRE_THROWS_AS(hist.fill(0, 1, -0.1), std::out_of_range); // col out of range
    REQUIRE_THROWS_AS(hist.fill(1, 0, 1.0), std::out_of_range);  // row out of range
    REQUIRE_THROWS_AS(hist.fill(58, -1, 1.1), std::out_of_range);  // both out of range

    auto v = hist.view();
    auto total = std::accumulate(v.begin(), v.end(), 0);
    REQUIRE(total == 0);

}


