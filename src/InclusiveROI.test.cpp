#include "aare/InclusiveROI.hpp"
#include <catch2/catch_test_macros.hpp>

using namespace aare;

TEST_CASE("mirror_on_y mirrors ROI about vertical axis",
          "[InlusiveROIgeometry]") {

    SECTION("ROI on left side of axis") {
        InclusiveROI roi{1, 3, 10, 20};

        auto mirrored = inclusiveroi::geom::mirror_on_y(roi, 5);

        REQUIRE(mirrored == InclusiveROI{6, 8, 10, 20});
    }
}