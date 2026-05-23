#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>


#include "test_config.hpp"
#include "test_macros.hpp"

#include "aare/PixelHistogram.hpp"

using aare::PixelHistogram;
using aare::NDArray;
using aare::NDView;

TEST_CASE("Fill one pixel of a 5x10 histogram"){
    PixelHistogram hist(5, 10, 20, 0.0, 10.0);
    NDArray<float, 2> image({5, 10}, -1.0); //Need to fill with -1 to not generate counts
    
    image(2, 3) = 5.7; // This should go into bin 11 (since bins are [0-0.5), [0.5-1.0), ..., [9.5-10.0))
    
    hist.fill(image.view());
    
    auto hdata = hist.hdata();
    REQUIRE(hdata.shape(0) == 5);
    REQUIRE(hdata.shape(1) == 10);
    REQUIRE(hdata.shape(2) == 20);
    
    // Check that the correct bin for pixel (2,3) has count 1
    CHECK(hdata(2, 3, 11) == 1);
    
    // Check that all other bins are zero
    for (ssize_t row = 0; row < hdata.shape(0); ++row) {
        for (ssize_t col = 0; col < hdata.shape(1); ++col) {
            for (ssize_t bin = 0; bin < hdata.shape(2); ++bin) {
                if (!(row == 2 && col == 3 && bin == 11)) {
                    CHECK(hdata(row, col, bin) == 0);
                }
            }
        }
    }
}

TEST_CASE("Fill pixels with uneven partial histogram row slices"){
    PixelHistogram hist(5, 4, 10, 0.0, 10.0, 3);
    NDArray<float, 2> image({5, 4}, -1.0);

    image(0, 0) = 0.2;
    image(1, 1) = 1.2;
    image(2, 2) = 2.2;
    image(3, 3) = 3.2;
    image(4, 0) = 4.2;

    hist.fill(image.view());

    auto hdata = hist.hdata();
    REQUIRE(hdata.shape(0) == 5);
    REQUIRE(hdata.shape(1) == 4);
    REQUIRE(hdata.shape(2) == 10);

    CHECK(hdata(0, 0, 0) == 1);
    CHECK(hdata(1, 1, 1) == 1);
    CHECK(hdata(2, 2, 2) == 1);
    CHECK(hdata(3, 3, 3) == 1);
    CHECK(hdata(4, 0, 4) == 1);

    for (ssize_t row = 0; row < hdata.shape(0); ++row) {
        for (ssize_t col = 0; col < hdata.shape(1); ++col) {
            for (ssize_t bin = 0; bin < hdata.shape(2); ++bin) {
                const bool expected = (row == 0 && col == 0 && bin == 0) ||
                                      (row == 1 && col == 1 && bin == 1) ||
                                      (row == 2 && col == 2 && bin == 2) ||
                                      (row == 3 && col == 3 && bin == 3) ||
                                      (row == 4 && col == 0 && bin == 4);
                if (!expected) {
                    CHECK(hdata(row, col, bin) == 0);
                }
            }
        }
    }
}
