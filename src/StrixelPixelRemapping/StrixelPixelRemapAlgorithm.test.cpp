#include "aare/StrixelPixelRemapping/StrixelPixelMap.hpp"
#include <catch2/catch_test_macros.hpp>

// Test structure:

// Geometry utilities
// │
// ├── translate() <-- InclusiveROI.test.cpp
// ├── mirror_on_x() <-- InclusiveROI.test.cpp
// ├── mirror_on_y() <-- InclusiveROI.test.cpp
// ├── mirrorXY() <-- InclusiveROI.test.cpp
// └── algo::detail::update_pixel_group_placement()
//        ├── identity
//        ├── bond shift
//        ├── rotation
//        └── shift + rotation

// Remapping algorithm
// │
// └── strixel_to_pixel_map()
//        ├── full map check for small group ROI (all hardcoded)
//        ├── full group exactly aligned (full map check)
//        ├── user ROI larger than group (full map check)
//        ├── user ROI smaller than group (full map check)
//        ├── user ROI partially overlaps group (full map ceck)
//        ├── user ROI does not intersect group (check empty)
//        ├── ModuloOrdering (check explicit, small map)
//        |       ├── Forward
//        |       └── Reverse
//        ├── update_pixel_group_placement (check integration)
//        |       ├── bond shift
//        |       └── Rotate180
//        ├── invalid multiplicity (check throws)
//        └── group width not divisible by multiplicity (check throws)

// For the full map checks, I have chosen to reimplement the maths behind the
// mapping algorithm instead of hardcoding maps. This can be debated.
// Crucially, I have isolated the maths of the mapping algorithm from the
// geometry utilities that are used. These are checked separately in
// InclusiveROI.test.cpp save for algo::detail::update_pixel_group_placement(),
// which combines geometry utilities and is checked here separately for geometry
// correctness and integration into the remapping algorithm. All geometry
// operations are hardcoded in the tests here.

// TODO: Implement tests for bond shift and rotation

using namespace aare;
using namespace aare::remap;

// Local helper definitions
namespace {

defs::SensorPixelGeometry test_sensor() {
    // return {.num_pix_x = 50, .num_pix_y = 50, .guardring = {0, 0}};
    return {50, 50, {0, 0}};
}

defs::SensorModulePlacement
test_placement(defs::Rotation rotation = defs::Rotation::Identity) {
    // return {.placement_on_module = {50, 99, 50, 99},
    //         .rotation = defs::Rotation::Identity};
    return {{50, 99, 50, 99}, rotation};
}

defs::GroupConfig
large_test_group(defs::ModuloOrdering ordering = defs::ModuloOrdering::Forward,
                 int multiplicity = 3,
                 InclusiveROI group_roi = {10, 39, 10, 39}) {
    // return {.strixel = {.multiplicity = 3, .pitch_um = 25.0},
    //         .routing = {.mod_order = ordering},
    //         .placement_on_sensor = {10, 39, 10, 39}};
    return {{multiplicity, 25.0}, {ordering}, group_roi};
}

defs::GroupConfig asymmetric_test_group() {
    // return {.strixel = {.multiplicity = 3, .pitch_um = 25.0},
    //         .routing = {defs::ModuloOrdering::Forward},
    //         .placement_on_sensor = {5, 13, 8, 16}};
    return {{3, 25.0}, {defs::ModuloOrdering::Forward}, {5, 13, 8, 16}};
}

defs::GroupConfig small_test_group() {
    // return {.strixel = {.multiplicity = 3, .pitch_um = 25.0},
    //         .routing = {defs::ModuloOrdering::Forward},
    //         .placement_on_sensor = {10, 15, 10, 11}};
    return {{3, 25.0}, {defs::ModuloOrdering::Forward}, {10, 15, 10, 11}};
}

} // namespace

/***************************************
 *
 * Geometry utilities
 * algo::detail::update_pixel_group_placement
 *
 ***************************************/
TEST_CASE("update_pixel_group_placement: shift and rotation geometry",
          "[remap][geometry][update_pixel_group_placement]") {

    const auto group = asymmetric_test_group();
    auto group_roi = group.placement_on_sensor;
    const auto sensor = test_sensor();

    SECTION("Identity reproduces itself") {

        auto updated = ::detail::update_pixel_group_placement(
            group_roi, sensor, {0, 0}, defs::Rotation::Identity);

        CHECK(updated == group_roi);
    }

    SECTION("Bond shift") {
        const defs::BondShift shift{2, 3};

        auto updated = detail::update_pixel_group_placement(
            group_roi, sensor, shift, defs::Rotation::Identity);

        // Original group: {5, 13, 8, 16}
        // After bond shift (+2, +3):
        //                 {7, 15, 11, 19}
        const InclusiveROI expected_roi{7, 15, 11, 19};

        CHECK(updated == expected_roi);
    }

    SECTION("180 degree rotation") {

        auto updated = detail::update_pixel_group_placement(
            group_roi, sensor, {0, 0}, defs::Rotation::Rotate180);

        // Original group: {5, 13, 8, 16}
        // After rotation in 50x50 sensor:
        //                 {36, 44, 33, 41}
        const InclusiveROI expected_roi{36, 44, 33, 41};

        CHECK(updated == expected_roi);
    }

    SECTION("bond shift and rotation") {
        const auto result = detail::update_pixel_group_placement(
            group_roi, sensor, {2, 3}, defs::Rotation::Rotate180);

        // Original group: {5, 13, 8, 16}
        // After bond shift (+2, +3):
        //                 {7, 15, 11, 19}
        // After rotation in 50x50 sensor:
        //                 {34, 43, 30, 38}
        const InclusiveROI expected_roi{34, 42, 30, 38};

        CHECK(result == expected_roi);
    }
}

/***************************************
 *
 * Remapping algorithm
 * strixel_to_pixel_map
 *
 ***************************************/

// small test group explicit mapping test
TEST_CASE("strixel_to_pixel_map: explicit mapping test with small ROI",
          "[remap][strixel_to_pixel_map]") {
    const auto group = small_test_group();
    const auto sensor = test_sensor();
    const auto placement = test_placement();

    // User ROI is in module coordinates, aligns with sensor
    const InclusiveROI user_roi{50, 99, 50, 99};

    StrixelPixelMap map(SensorConfig<1>{sensor, {group}}, placement, {0, 0});

    map.calculate_map_from_roi(user_roi);

    const auto result = map.get_group_maps()[0];

    const InclusiveROI expected_roi{10, 15, 10, 11};

    CHECK(result.effective_roi == expected_roi);
    CHECK(result.map.shape(0) == 6);
    CHECK(result.map.shape(1) == 2);

    // Check every pixel explicitely
    // First pixel row
    CHECK(result.map(0, 0) == 510); // pixel (10, 10)
    CHECK(result.map(1, 0) == 511); // pixel (10, 11)
    CHECK(result.map(2, 0) == 512); // pixel (10, 12)
    CHECK(result.map(0, 1) == 513); // pixel (10, 13)
    CHECK(result.map(1, 1) == 514); // pixel (10, 14)
    CHECK(result.map(2, 1) == 515); // pixel (10, 15)

    // Second pixel row + user_roi.width()
    CHECK(result.map(3, 0) == 560); // pixel (11, 10)
    CHECK(result.map(4, 0) == 561); // pixel (11, 11)
    CHECK(result.map(5, 0) == 562); // pixel (11, 12)
    CHECK(result.map(3, 1) == 563); // pixel (11, 13)
    CHECK(result.map(4, 1) == 564); // pixel (11, 14)
    CHECK(result.map(5, 1) == 565); // pixel (11, 15)
}

// full group exactly aligned
TEST_CASE("strixel_to_pixel_map: user ROI exactly aligned with group ROI",
          "[remap][strixel_to_pixel_map]") {

    const auto group = large_test_group();
    const auto sensor = test_sensor();
    const auto placement = test_placement();

    // User ROI is in module coordinates.
    const InclusiveROI user_roi{60, 89, 60, 89};

    StrixelPixelMap map(SensorConfig<1>{sensor, {group}}, placement, {0, 0});

    map.calculate_map_from_roi(user_roi);

    const auto result = map.get_group_maps()[0];

    // ROI aligns exactly with group
    const InclusiveROI expected_roi = group.placement_on_sensor;

    CHECK(result.effective_roi == expected_roi);
    CHECK(result.map.shape(0) == 90);
    CHECK(result.map.shape(1) == 10);

    // Check some explicit values
    CHECK(result.map(0, 0) == 0); // pixel(0, 0)
    CHECK(result.map(1, 0) == 1); // pixel(0, 1)
    CHECK(result.map(2, 0) == 2); // pixel(0, 2)
    CHECK(result.map(0, 1) == 3); // pixel(0, 3)
    CHECK(result.map(1, 1) == 4); // pixel(0, 4)
    CHECK(result.map(2, 1) == 5); // pixel(0, 5)

    CHECK(result.map(89, 9) == 899); // pixel(29, 29)
    CHECK(result.map(88, 9) == 898); // pixel(29, 28)
    CHECK(result.map(87, 9) == 897); // pixel(29, 27)
    CHECK(result.map(89, 8) == 896); // pixel(29, 26)
    CHECK(result.map(88, 8) == 895); // pixel(29, 25)
    CHECK(result.map(87, 8) == 894); // pixel(29, 24)
}

// user ROI larger than group (and larger than sensor)
TEST_CASE("strixel_to_pixel_map: user ROI larger than group ROI",
          "[remap][strixel_to_pixel_map]") {

    const auto group = small_test_group();
    const auto sensor = test_sensor();
    const auto placement = test_placement();

    // User ROI is in module coordinates.
    const InclusiveROI user_roi{45, 104, 45, 104};

    StrixelPixelMap map(SensorConfig<1>{sensor, {group}}, placement, {0, 0});

    map.calculate_map_from_roi(user_roi);

    const auto result = map.get_group_maps()[0];

    const InclusiveROI expected_roi = group.placement_on_sensor;

    CHECK(result.effective_roi == expected_roi);
    CHECK(result.map.shape(0) == 6);
    CHECK(result.map.shape(1) == 2);

    // Check every pixel explicitely
    // First pixel row
    CHECK(result.map(0, 0) == 915); // pixel (15, 15)
    CHECK(result.map(1, 0) == 916); // pixel (15, 16)
    CHECK(result.map(2, 0) == 917); // pixel (15, 17)
    CHECK(result.map(0, 1) == 918); // pixel (15, 18)
    CHECK(result.map(1, 1) == 919); // pixel (15, 19)
    CHECK(result.map(2, 1) == 920); // pixel (15, 20)

    // Second pixel row + user_roi.width()
    CHECK(result.map(3, 0) == 975); // pixel (16, 15)
    CHECK(result.map(4, 0) == 976); // pixel (16, 16)
    CHECK(result.map(5, 0) == 977); // pixel (16, 17)
    CHECK(result.map(3, 1) == 978); // pixel (16, 18)
    CHECK(result.map(4, 1) == 979); // pixel (16, 19)
    CHECK(result.map(5, 1) == 980); // pixel (16, 20)
}

// user ROI smaller than group
TEST_CASE("strixel_to_pixel_map: user ROI smaller than group ROI",
          "[remap][strixel_to_pixel_map]") {

    const auto group = large_test_group();
    const auto sensor = test_sensor();
    const auto placement = test_placement();

    // User ROI is in module coordinates.
    const InclusiveROI user_roi{65, 70, 65, 66};

    StrixelPixelMap map(SensorConfig<1>{sensor, {group}}, placement, {0, 0});

    map.calculate_map_from_roi(user_roi);

    const auto result = map.get_group_maps()[0];

    // Now expected_roi is given by the smaller user ROI (in sensor-local
    // coordinates)
    // Expected intersection:
    //
    // user ROI, sensor-local:  {15, 20, 15, 16}
    // group ROI, sensor-local: {10, 39, 10, 39}
    //                          ----------------
    // effective ROI:           {15, 20, 15, 16}
    const InclusiveROI expected_roi = {15, 20, 15, 16};

    CHECK(result.effective_roi == expected_roi);

    // The map shape is now determined by the smaller user ROI in relation to
    // the group ROI:
    //  - The group ROI still determines the modulo progression and where
    //    multiplicity groups start
    //  - The smaller user ROI determines from where we start looking
    //  - The algorithm makes sure that the map shape is always large enough so
    //    that always full multiplicity groups are contained
    //    (In this concrete example:
    //       > width of user_roi is 6
    //       > 6/multiplicity = 2
    //       > map width of 2 should cover it?
    //       > BUT: the placement with respect to the group_roi matters!
    //       > Since the group multiplicity chunks are not fully covered by the
    //       ROI, the map will be larger > Hence the width of the map becomes 3
    //  - Entries that are contained in the map but not in the user ROI, will be
    //    mapped to -1
    CHECK(result.map.shape(0) == 6);
    CHECK(result.map.shape(1) == 3);

    // Check every pixel explicitely
    // First pixel row
    CHECK(result.map(0, 0) == -1); // pixel (0, -2) - not in user_roi
    CHECK(result.map(1, 0) == -1); // pixel (0, -1) - not in user_roi
    CHECK(result.map(2, 0) == 0);  // pixel (0, 0)
    CHECK(result.map(0, 1) == 1);  // pixel (0, 1)
    CHECK(result.map(1, 1) == 2);  // pixel (0, 2)
    CHECK(result.map(2, 1) == 3);  // pixel (0, 3)
    CHECK(result.map(0, 2) == 4);  // pixel (0, 4)
    CHECK(result.map(1, 2) == 5);  // pixel (0, 5)
    CHECK(result.map(2, 2) == -1); // pixel (0, 6) - not in user_roi

    // Second pixel row
    CHECK(result.map(3, 0) == -1); // pixel (1, -2) - not in user_roi
    CHECK(result.map(4, 0) == -1); // pixel (1, -1) - not in user_roi
    CHECK(result.map(5, 0) == 6);  // pixel (1, 0)
    CHECK(result.map(3, 1) == 7);  // pixel (1, 1)
    CHECK(result.map(4, 1) == 8);  // pixel (1, 2)
    CHECK(result.map(5, 1) == 9);  // pixel (1, 3)
    CHECK(result.map(3, 2) == 10); // pixel (1, 4)
    CHECK(result.map(4, 2) == 11); // pixel (1, 5)
    CHECK(result.map(5, 2) == -1); // pixel (1, 6) - not in user_roi
}

// user ROI partially overlaps group
TEST_CASE("strixel_to_pixel_map: user ROI partially overlaps",
          "[remap][strixel_to_pixel_map]") {

    const auto group = large_test_group();
    const auto sensor = test_sensor();
    const auto placement = test_placement();

    // User ROI is in module coordinates.
    // In sensor-local coordinates this is {5, 13, 5, 11}.
    const InclusiveROI user_roi{55, 63, 55, 61};

    StrixelPixelMap map(SensorConfig<1>{sensor, {group}}, placement, {0, 0});

    map.calculate_map_from_roi(user_roi);

    const auto result = map.get_group_maps()[0];

    // Expected intersection:
    //
    // user ROI, sensor-local: { 5, 13,  5, 11}
    // group ROI:              {10, 39, 10, 39}
    //                         ----------------
    // effective ROI:          {10, 13, 10, 11}
    const InclusiveROI expected_roi{10, 13, 10, 11};

    CHECK(result.effective_roi == expected_roi);
    CHECK(result.map.shape(0) == 6);
    CHECK(result.map.shape(1) == 2);

    // Check every pixel explicitely
    // First pixel row
    CHECK(result.map(0, 0) == 50); // pixel (5, 5)
    CHECK(result.map(1, 0) == 51); // pixel (5, 6)
    CHECK(result.map(2, 0) == 52); // pixel (5, 7)
    CHECK(result.map(0, 1) == 53); // pixel (5, 8)
    CHECK(result.map(1, 1) == -1); // pixel (5, 9) - not in user_roi
    CHECK(result.map(2, 1) == -1); // pixel (5, 10) - not in user_roi

    // Second pixel row
    CHECK(result.map(3, 0) == 59); // pixel (6, 5)
    CHECK(result.map(4, 0) == 60); // pixel (6, 6)
    CHECK(result.map(5, 0) == 61); // pixel (6, 7)
    CHECK(result.map(3, 1) == 62); // pixel (6, 8)
    CHECK(result.map(4, 1) == -1); // pixel (6, 9) - not in user_roi
    CHECK(result.map(5, 1) == -1); // pixel (6, 10) - not in user_roi
}

// user ROI does not intersect group
TEST_CASE("strixel_to_pixel_map: user ROI does not intersect group ROI",
          "[remap][strixel_to_pixel_map]") {

    const auto group = large_test_group();
    const auto sensor = test_sensor();
    const auto placement = test_placement();

    const InclusiveROI user_roi{10, 59, 10, 84};

    StrixelPixelMap map(SensorConfig<1>{sensor, {group}}, placement, {0, 0});

    map.calculate_map_from_roi(user_roi);

    const auto result = map.get_group_maps()[0];

    CHECK(result.effective_roi.is_empty());
    CHECK(result.map.data() ==
          nullptr); // is there a better check for empty NDArray?
}

// ModuloOrdering
TEST_CASE("strixel_to_pixel_map: modulo ordering",
          "[remap][strixel_to_pixel_map]") {

    const auto sensor = test_sensor();
    const auto placement = test_placement();

    const InclusiveROI user_roi{60, 62, 60, 62};

    SECTION("Forward ordering") {
        const auto forward_group =
            large_test_group(defs::ModuloOrdering::Forward);

        StrixelPixelMap map(SensorConfig<1>{sensor, {forward_group}}, placement,
                            {0, 0});

        map.calculate_map_from_roi(user_roi);

        const auto result = map.get_group_maps()[0];

        CHECK(result.map.shape(0) == 9);
        CHECK(result.map.shape(1) == 1);

        // Pixels x=60,61,62 correspond to modulo values 0,1,2.
        //
        // Forward ordering:
        //   mod 0 -> strixel row 0
        //   mod 1 -> strixel row 1
        //   mod 2 -> strixel row 2

        CHECK(result.map(0, 0) == 0);
        CHECK(result.map(1, 0) == 1);
        CHECK(result.map(2, 0) == 2);
        CHECK(result.map(3, 0) == 3);
        CHECK(result.map(4, 0) == 4);
        CHECK(result.map(5, 0) == 5);
        CHECK(result.map(6, 0) == 6);
        CHECK(result.map(7, 0) == 7);
        CHECK(result.map(8, 0) == 8);
    }

    SECTION("Reverse ordering") {
        const auto reverse_group =
            large_test_group(defs::ModuloOrdering::Reverse);

        StrixelPixelMap map(SensorConfig<1>{sensor, {reverse_group}}, placement,
                            {0, 0});

        map.calculate_map_from_roi(user_roi);

        const auto result = map.get_group_maps()[0];

        CHECK(result.map.shape(0) == 9);
        CHECK(result.map.shape(1) == 1);

        // Reverse ordering:
        //   mod 0 -> strixel row 2
        //   mod 1 -> strixel row 1
        //   mod 2 -> strixel row 0

        CHECK(result.map(0, 0) == 2);
        CHECK(result.map(0, 1) == 1);
        CHECK(result.map(0, 2) == 0);
        CHECK(result.map(3, 0) == 5);
        CHECK(result.map(4, 0) == 4);
        CHECK(result.map(5, 0) == 3);
        CHECK(result.map(6, 0) == 8);
        CHECK(result.map(7, 0) == 7);
        CHECK(result.map(8, 0) == 6);
    }
}

// update_pixel_group_placement integration
TEST_CASE(
    "strixel_to_pixel_map: update_pixel_group_placement integration",
    "[remap][strixel_to_pixel_map][geometry][update_pixel_group_placement]") {

    const auto group = asymmetric_test_group();
    const auto sensor = test_sensor();
    const auto placement = test_placement();

    // user_roi = sensor_roi
    const InclusiveROI user_roi = placement.placement_on_module;

    // Rebase into sensor roi:
    // {50, 99, 50, 99}
    const InclusiveROI user_roi_local = {0, 49, 0, 49};

    // Pixel index helper (for readability)
    const auto expected_pixel = [&](ssize_t x, ssize_t y) {
        return (y - user_roi_local.ymin) * user_roi_local.width() +
               (x - user_roi_local.xmin);
    };

    SECTION("bond shift integrates correctly") {
        const defs::BondShift shift{2, 3};

        StrixelPixelMap map(SensorConfig<1>{sensor, {group}}, placement, shift);

        map.calculate_map_from_roi(user_roi);

        const auto result = map.get_group_maps()[0];

        // Original group: {5, 13, 8, 16}
        // After bond shift (+2, +3):
        //                 {7, 15, 11, 19}
        const InclusiveROI expected_roi{7, 15, 11, 19};

        CHECK(result.effective_roi == expected_roi);
        CHECK(result.map.shape(0) == 27);
        CHECK(result.map.shape(1) == 3);

        // Bottom-left pixel of the shifted group.
        // x=7, y=11 -> dx=0, dy=0 -> strixel (0,0)
        CHECK(result.map(0, 0) == expected_pixel(7, 11));

        // Same pixel row, different modulo position.
        // x=9, y=11 -> dx=2 -> strixel (2,0)
        CHECK(result.map(2, 0) == expected_pixel(9, 11));

        // A pixel from a later row and column.
        // x=13, y=15 -> dx=6, dy=4 -> strixel (12,2)
        CHECK(result.map(12, 2) == expected_pixel(13, 15));
    }

    SECTION("rotation integrates correctly") {
        const auto rotated_placement =
            test_placement(defs::Rotation::Rotate180);

        StrixelPixelMap map(SensorConfig<1>{sensor, {group}}, rotated_placement,
                            {0, 0});

        map.calculate_map_from_roi(user_roi);

        const auto result = map.get_group_maps()[0];

        // Original group: {5, 13, 8, 16}
        // After rotation in 50x50 sensor:
        //                 {36, 44, 33, 41}
        const InclusiveROI expected_roi{36, 44, 33, 41};

        CHECK(result.effective_roi == expected_roi);
        CHECK(result.map.shape(0) == 27);
        CHECK(result.map.shape(1) == 3);

        // Bottom-left pixel of the shifted group.
        // x=36, y=33 -> dx=0, dy=0 -> strixel (0,0)
        CHECK(result.map(0, 0) == expected_pixel(36, 33));

        // Same pixel row, different modulo position.
        // x=38, y=33 -> dx=2 -> strixel (2,0)
        CHECK(result.map(2, 0) == expected_pixel(38, 33));

        // A pixel from a later row and column.
        // x=42, y=37 -> dx=6, dy=4 -> strixel (12,2)
        CHECK(result.map(12, 2) == expected_pixel(42, 37));
    }
}

// invalid multiplicity
TEST_CASE("strixel_to_pixel_map: invalid multiplicity",
          "[remap][strixel_to_pixel_map]") {

    const defs::ModuloOrdering ordering = defs::ModuloOrdering::Forward;
    const int multiplicity = 0;

    const auto group = large_test_group(ordering, multiplicity);
    const auto sensor = test_sensor();
    const auto placement = test_placement();

    const InclusiveROI user_roi{10, 59, 10, 84};

    StrixelPixelMap map(SensorConfig<1>{sensor, {group}}, placement, {0, 0});

    CHECK_THROWS_AS(map.calculate_map_from_roi(user_roi), std::logic_error);
}

// group width not divisible by multiplicity
TEST_CASE("strixel_to_pixel_map: group width not divisible by multiplicity",
          "[remap][strixel_to_pixel_map]") {

    const defs::ModuloOrdering ordering = defs::ModuloOrdering::Forward;
    const int multiplicity = 3;
    const InclusiveROI group_roi = {11, 39, 10, 39}; // width = 29

    const auto group = large_test_group(ordering, multiplicity, group_roi);
    const auto sensor = test_sensor();
    const auto placement = test_placement();

    const InclusiveROI user_roi{10, 59, 10, 84};

    StrixelPixelMap map(SensorConfig<1>{sensor, {group}}, placement, {0, 0});

    CHECK_THROWS_AS(map.calculate_map_from_roi(user_roi), std::logic_error);
}