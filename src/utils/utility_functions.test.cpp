#include "aare/utils/utility_functions.hpp"
#include <catch2/catch_test_macros.hpp>

namespace aare {

TEST_CASE("merge ROIs", "[utility_functions]") {

    SECTION("not fully contiguous") {

        std::vector<ROI> rois = {ROI{20, 30, 50, 60}, ROI{20, 30, 40, 50},
                                 ROI{10, 20, 40, 50}};

        auto merged_rois = merge_consecutive_rois<false, false>(rois);

        REQUIRE(merged_rois.size() == 2);

        REQUIRE(merged_rois[0] == ROI{10, 30, 40, 50});
        REQUIRE(merged_rois[1] == ROI{20, 30, 50, 60});
    }
    SECTION("complex merge") {

        std::vector<ROI> rois = {ROI{40, 50, 20, 30}, ROI{10, 20, 30, 40},
                                 ROI{10, 20, 50, 60}, ROI{20, 30, 30, 40},
                                 ROI{60, 70, 30, 40}, ROI{60, 70, 20, 30},
                                 ROI{20, 30, 20, 30}, ROI{10, 20, 20, 30}};

        auto merged_rois = merge_consecutive_rois<false, false>(rois);

        REQUIRE(merged_rois.size() == 4);

        REQUIRE(merged_rois[0] == ROI{10, 30, 20, 40});
        REQUIRE(merged_rois[1] == ROI{10, 20, 50, 60});
        REQUIRE(merged_rois[2] == ROI{40, 50, 20, 30});
        REQUIRE(merged_rois[3] == ROI{60, 70, 20, 40});
    }
    SECTION("horizontally aligned") {
        std::vector<ROI> rois = {ROI{10, 20, 30, 40}, ROI{10, 20, 50, 60},
                                 ROI{10, 20, 20, 30}};

        auto merged_rois = merge_consecutive_rois<true, false>(rois);

        REQUIRE(merged_rois.size() == 2);
        REQUIRE(merged_rois[0] == ROI{10, 20, 20, 40});
        REQUIRE(merged_rois[1] == ROI{10, 20, 50, 60});
    }
    SECTION("vertically aligned") {
        std::vector<ROI> rois = {ROI{10, 20, 30, 40}, ROI{30, 40, 30, 40},
                                 ROI{20, 30, 30, 40}};

        auto merged_rois = merge_consecutive_rois<false, true>(rois);

        REQUIRE(merged_rois.size() == 1);
        REQUIRE(merged_rois[0] == ROI{10, 40, 30, 40});
    }
}

} // namespace aare
