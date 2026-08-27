#include "aare/StrixelPixelRemapAlgorithm.hpp"
#include <catch2/catch_test_macros.hpp>

// strixel_to_pixel_map
// ├── full group exactly aligned (full map check)
// ├── user ROI larger than group (full map check)
// ├── user ROI smaller than group (full map check)
// ├── user ROI partially overlaps group (full map ceck)
// ├── user ROI does not intersect group (full map check)
// ├── ModuloOrdering::Reverse (test Forward vs Reverse, only a few pixels) ?
// ├── bond shift ?
// ├── rotation ?
// ├── invalid multiplicity (check thows)
// └── group width not divisible by multiplicity (check throws)

// For the full map checks, I have chosen to reimplement the maths behind the
// mapping algorithm instead of hardcoding maps. This can be debated.
// Crucially, I have isolated the maths of the mapping algorithm from the
// geometry utilities that are used. These are checked separately in
// InclusiveROI.test.cpp. All geometry operations are hardcoded in the tests
// here.

// TODO: Implement tests for ModuloOrdering, bond shift, and rotation

using namespace aare;
using namespace aare::remap;

// Local helper definitions
namespace {

defs::SensorPixelGeometry test_sensor() {
    // return {.num_pix_x = 50, .num_pix_y = 50, .guardring = {0, 0}};
    return {50, 50, {0, 0}};
}

defs::SensorModulePlacement test_placement() {
    // return {.placement_on_module = {50, 99, 50, 99},
    //         .rotation = defs::Rotation::Identity};
    return {{50, 99, 50, 99}, defs::Rotation::Identity};
}

defs::GroupConfig
test_group(defs::ModuloOrdering ordering = defs::ModuloOrdering::Forward,
           int multiplicity = 3, InclusiveROI group_roi = {10, 39, 10, 39}) {
    // return {.strixel = {.multiplicity = 3, .pitch_um = 25.0},
    //         .routing = {.mod_order = ordering},
    //         .placement_on_sensor = {10, 39, 10, 39}};
    return {{multiplicity, 25.0}, {ordering}, group_roi};
}

} // namespace

// full group exactly aligned
TEST_CASE("strixel_to_pixel_map: user ROI exactly aligned with group ROI",
          "[remap][strixel_to_pixel_map]") {

    const auto group = test_group();
    const auto sensor = test_sensor();
    const auto placement = test_placement();

    // User ROI is in module coordinates.
    const InclusiveROI user_roi{60, 89, 60, 89};

    // User ROI fully covers group ROI
    const InclusiveROI user_roi_local =
        group.placement_on_sensor; // {10, 39, 10, 39}

    const auto result =
        algo::strixel_to_pixel_map(group, sensor, placement, user_roi, {0, 0});

    // The group occupies [10,39] x [10,39] in sensor-local coordinates.
    const InclusiveROI expected_roi{10, 39, 10, 39};

    CHECK(result.effective_roi == expected_roi);
    CHECK(result.map.shape(0) == 90);
    CHECK(result.map.shape(1) == 10);

    /*
     * Mathematical definition of the expected mapping:
     *
     *   sensor-local pixel:
     *       (x, y)
     *
     *   corresponding user-ROI pixel:
     *       (x - user_roi_local.xmin) +
     *       (y - user_roi_local.ymin) * user_roi_local.width()
     *
     *   where user_roi_local is the user ROI rebased from module
     *   coordinates into sensor coordinates.
     *
     *   For Forward ordering:
     *
     *       dx  = x - group.xmin
     *       dy  = y - group.ymin
     *       mod = dx % multiplicity
     *
     *       strixel_col = dx / multiplicity
     *       strixel_row = dy * multiplicity + mod
     *
     * This test deliberately calculates the expected values directly
     * from those definitions rather than using the implementation's
     * intermediate calculations.
     */
    for (ssize_t y = result.effective_roi.ymin; y <= result.effective_roi.ymax;
         ++y) {

        for (ssize_t x = result.effective_roi.xmin;
             x <= result.effective_roi.xmax; ++x) {

            const ssize_t dx = x - expected_roi.xmin;
            const ssize_t dy = y - expected_roi.ymin;

            const ssize_t strixel_row = dy * group.strixel.multiplicity +
                                        dx % group.strixel.multiplicity;

            const ssize_t strixel_col = dx / group.strixel.multiplicity;

            const ssize_t expected_pixel =
                (y - user_roi_local.ymin) * user_roi_local.width() +
                (x - user_roi_local.xmin);

            CHECK(result.map(strixel_row, strixel_col) == expected_pixel);
        }
    }
}

// most important, most generic test (emulating most likely reality)
// user ROI larger than group (and larger than sensor)
TEST_CASE("strixel_to_pixel_map: user ROI larger than group ROI",
          "[remap][strixel_to_pixel_map]") {

    const auto group = test_group();
    const auto sensor = test_sensor();
    const auto placement = test_placement();

    // User ROI is in module coordinates.
    const InclusiveROI user_roi{45, 104, 45, 104};

    const auto result =
        algo::strixel_to_pixel_map(group, sensor, placement, user_roi, {0, 0});

    // The group occupies [10,39] x [10,39] in sensor-local coordinates.
    const InclusiveROI expected_roi{10, 39, 10, 39};

    CHECK(result.effective_roi == expected_roi);
    CHECK(result.map.shape(0) == 90);
    CHECK(result.map.shape(1) == 10);

    // Rebase into sensor roi:
    // {50, 99, 50, 99}
    const InclusiveROI user_roi_local = {-5, 54, -5, 54};

    for (ssize_t y = result.effective_roi.ymin; y <= result.effective_roi.ymax;
         ++y) {

        for (ssize_t x = result.effective_roi.xmin;
             x <= result.effective_roi.xmax; ++x) {

            const ssize_t dx = x - expected_roi.xmin;
            const ssize_t dy = y - expected_roi.ymin;

            const ssize_t strixel_row = dy * group.strixel.multiplicity +
                                        dx % group.strixel.multiplicity;

            const ssize_t strixel_col = dx / group.strixel.multiplicity;

            const ssize_t expected_pixel =
                (y - user_roi_local.ymin) * user_roi_local.width() +
                (x - user_roi_local.xmin);

            CHECK(result.map(strixel_row, strixel_col) == expected_pixel);
        }
    }
}

// user ROI smaller than group
TEST_CASE("strixel_to_pixel_map: user ROI smaller than group ROI",
          "[remap][strixel_to_pixel_map]") {

    const auto group = test_group();
    const auto sensor = test_sensor();
    const auto placement = test_placement();

    // User ROI is in module coordinates.
    const InclusiveROI user_roi{65, 84, 65, 84};

    const auto result =
        algo::strixel_to_pixel_map(group, sensor, placement, user_roi, {0, 0});

    // Now expected_roi is given by the smaller user ROI (in sensor-local
    // coordinates)
    // Expected intersection:
    //
    // user ROI, sensor-local:  {15, 34, 15, 34}
    // group ROI, sensor-local: {10, 39, 10, 39}
    //                          ----------------
    // effective ROI:           {15, 34, 15, 34}
    const InclusiveROI expected_roi = {15, 34, 15, 34};

    CHECK(result.effective_roi == expected_roi);

    // The map shape is now determined by the smaller user ROI in relation to
    // the group ROI:
    //  - The group ROI still determines the modulo progression and where
    //    multiplicity groups start
    //  - The smaller user ROI determines from where we start looking
    //  - The algorithm makes sure that the map shape is always large enough so
    //    that always full multiplicity groups are contained
    //    (In this concrete example:
    //       > width of user_roi is 20
    //       > 20/multiplicity = 6 (plus rest)
    //       > map width of 7 should, in principle, cover it
    //       > BUT: the placement with respect to the group_roi matters!
    //       > Since both pixels at x = 15 and x = 34 are part of their own
    //         separate multiplicity group, we must add 2 additional
    //         multiplicity groups to the map
    //       > Hence the width of the map becomes 8
    //    )
    //  - Entries that are contained in the map but not in the user ROI, will be
    //    mapped to -1
    CHECK(result.map.shape(0) == 60);
    CHECK(result.map.shape(1) == 8);

    // Explicit check not mapped example
    CHECK(result.map(0, 0) == -1); // not mapped because not in user_roi!

    // Now we need to establish the valid ROI within the map IN THE MAP SPACE
    // (i.e. in strixel coordinates)
    // Map starts at (0,0), but the first valid column is min_col = 1!
    // (15-10 = 5, 5/3 = 1 (plus rest)
    // First valid row becomes 15-10 = 5 -> 5*3 = 15
    constexpr ssize_t expected_min_row = 15;
    constexpr ssize_t expected_min_col = 1;

    constexpr ssize_t expected_user_roi_local_xmin = 15;
    constexpr ssize_t expected_user_roi_local_ymin = 15;

    // Check the whole map!
    // Also, shift strixel row and cols according to min_row/min_cols
    for (ssize_t map_row = 0; map_row < result.map.shape(0); ++map_row) {
        for (ssize_t map_col = 0; map_col < result.map.shape(1); ++map_col) {

            const ssize_t strixel_row = expected_min_row + map_row;
            const ssize_t strixel_col = expected_min_col + map_col;

            const ssize_t dy = strixel_row / group.strixel.multiplicity;
            const ssize_t mod = strixel_row % group.strixel.multiplicity;
            const ssize_t dx = strixel_col * group.strixel.multiplicity + mod;

            const ssize_t x = group.placement_on_sensor.xmin + dx;
            const ssize_t y = group.placement_on_sensor.ymin + dy;

            if (expected_roi.contains(x, y)) {
                const ssize_t expected_pixel =
                    (y - expected_user_roi_local_ymin) * user_roi.width() +
                    (x - expected_user_roi_local_xmin);

                CHECK(result.map(map_row, map_col) == expected_pixel);
            } else {
                // pixel not mapped
                CHECK(result.map(map_row, map_col) == -1);
            }
        }
    }
}

// user ROI partially overlaps group
TEST_CASE("strixel_to_pixel_map: user ROI partially overlaps",
          "[remap][strixel_to_pixel_map]") {

    const auto group = test_group();
    const auto sensor = test_sensor();
    const auto placement = test_placement();

    // User ROI is in module coordinates.
    // In sensor-local coordinates this is {5, 34, 5, 34}.
    const InclusiveROI user_roi{55, 84, 55, 84};

    // For reference:
    // user_roi for full covered group = {60, 89, 60, 89};
    // group roi in sensor coordinates = {10, 39, 10, 39};

    const auto result =
        algo::strixel_to_pixel_map(group, sensor, placement, user_roi, {0, 0});

    // Expected intersection:
    //
    // user ROI, sensor-local: { 5, 34,  5, 34}
    // group ROI:              {10, 39, 10, 39}
    //                         ----------------
    // effective ROI:          {10, 34, 10, 34}
    const InclusiveROI expected_roi{10, 34, 10, 34};

    CHECK(result.effective_roi == expected_roi);
    CHECK(result.map.shape(0) == 75); // 25 pixel rows * 3
    CHECK(result.map.shape(1) ==
          9); // Pixel 34 is contained in the 9th multiplicity group

    constexpr ssize_t expected_user_roi_local_xmin = 5;
    constexpr ssize_t expected_user_roi_local_ymin = 5;

    for (ssize_t map_row = 0; map_row < result.map.shape(0); ++map_row) {
        for (ssize_t map_col = 0; map_col < result.map.shape(1); ++map_col) {

            // Map coordinates coincide with the strixel coordinates here
            // because min_row == min_col == 0.
            const ssize_t strixel_row = map_row;
            const ssize_t strixel_col = map_col;

            const ssize_t dy = strixel_row / group.strixel.multiplicity;
            const ssize_t mod = strixel_row % group.strixel.multiplicity;
            const ssize_t dx = strixel_col * group.strixel.multiplicity + mod;

            const ssize_t x = group.placement_on_sensor.xmin + dx;
            const ssize_t y = group.placement_on_sensor.ymin + dy;

            if (expected_roi.contains(x, y)) {
                const ssize_t expected_pixel =
                    (y - expected_user_roi_local_ymin) * user_roi.width() +
                    (x - expected_user_roi_local_xmin);

                CHECK(result.map(map_row, map_col) == expected_pixel);
            } else {
                CHECK(result.map(map_row, map_col) == -1);
            }
        }
    }
}

// user ROI does not intersect group
TEST_CASE("strixel_to_pixel_map: user ROI does not intersect group ROI",
          "[remap][strixel_to_pixel_map]") {

    const auto group = test_group();
    const auto sensor = test_sensor();
    const auto placement = test_placement();

    const InclusiveROI user_roi{10, 59, 10, 84};

    const auto result =
        algo::strixel_to_pixel_map(group, sensor, placement, user_roi, {0, 0});

    CHECK(result.effective_roi.is_empty());
    CHECK(result.map.data() ==
          nullptr); // is there a better check for empty NDArray?
}

// invalid multiplicity
TEST_CASE("strixel_to_pixel_map: invalid multiplicity",
          "[remap][strixel_to_pixel_map]") {

    const defs::ModuloOrdering ordering = defs::ModuloOrdering::Forward;
    const int multiplicity = 0;

    const auto group = test_group(ordering, multiplicity);
    const auto sensor = test_sensor();
    const auto placement = test_placement();

    const InclusiveROI user_roi{10, 59, 10, 84};

    CHECK_THROWS_AS(
        algo::strixel_to_pixel_map(group, sensor, placement, user_roi, {0, 0}),
        std::logic_error);
}

// group width not divisible by multiplicity
TEST_CASE("strixel_to_pixel_map: group width not divisible by multiplicity",
          "[remap][strixel_to_pixel_map]") {

    const defs::ModuloOrdering ordering = defs::ModuloOrdering::Forward;
    const int multiplicity = 3;
    const InclusiveROI group_roi = {11, 39, 10, 39}; // width = 29

    const auto group = test_group(ordering, multiplicity, group_roi);
    const auto sensor = test_sensor();
    const auto placement = test_placement();

    const InclusiveROI user_roi{10, 59, 10, 84};

    CHECK_THROWS_AS(
        algo::strixel_to_pixel_map(group, sensor, placement, user_roi, {0, 0}),
        std::logic_error);
}