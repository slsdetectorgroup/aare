#include "aare/defs.hpp"

#include <catch2/catch_test_macros.hpp>

TEST_CASE("Enum values") {
    // Since some of the enums are written to file we need to make sure
    // they match the value in the slsDetectorPackage

    REQUIRE(static_cast<int>(aare::DetectorType::Generic) == 0);
    REQUIRE(static_cast<int>(aare::DetectorType::Eiger) == 1);
    REQUIRE(static_cast<int>(aare::DetectorType::Gotthard) == 2);
    REQUIRE(static_cast<int>(aare::DetectorType::Jungfrau) == 3);
    REQUIRE(static_cast<int>(aare::DetectorType::ChipTestBoard) == 4);
    REQUIRE(static_cast<int>(aare::DetectorType::Moench) == 5);
    REQUIRE(static_cast<int>(aare::DetectorType::Mythen3) == 6);
    REQUIRE(static_cast<int>(aare::DetectorType::Gotthard2) == 7);
    REQUIRE(static_cast<int>(aare::DetectorType::Xilinx_ChipTestBoard) == 8);

    // Not included
    REQUIRE(static_cast<int>(aare::DetectorType::Moench03) == 100);
}

TEST_CASE("DynamicCluster creation") {
    aare::DynamicCluster c(13, 15);
    REQUIRE(c.cluster_sizeX == 13);
    REQUIRE(c.cluster_sizeY == 15);
    REQUIRE(c.dt == aare::Dtype(typeid(int32_t)));
    REQUIRE(c.data() != nullptr);

    aare::DynamicCluster c2(c);
    REQUIRE(c2.cluster_sizeX == 13);
    REQUIRE(c2.cluster_sizeY == 15);
    REQUIRE(c2.dt == aare::Dtype(typeid(int32_t)));
    REQUIRE(c2.data() != nullptr);
}

// TEST_CASE("cluster set and get data") {

//     aare::DynamicCluster c2(33, 44, aare::Dtype(typeid(double)));
//     REQUIRE(c2.cluster_sizeX == 33);
//     REQUIRE(c2.cluster_sizeY == 44);
//     REQUIRE(c2.dt == aare::Dtype::DOUBLE);
//     double v = 3.14;
//     c2.set<double>(0, v);
//     double v2 = c2.get<double>(0);
//     REQUIRE(aare::compare_floats<double>(v, v2));

//     c2.set<double>(33 * 44 - 1, 123.11);
//     double v3 = c2.get<double>(33 * 44 - 1);
//     REQUIRE(aare::compare_floats<double>(123.11, v3));

//     REQUIRE_THROWS_AS(c2.set(0, 1), std::invalid_argument); // set int to
//     double
// }