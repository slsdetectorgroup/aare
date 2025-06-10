#include "aare/defs.hpp"

#include <catch2/catch_test_macros.hpp>
#include <string>

using aare::StringTo;
using aare::ToString;

TEST_CASE("Enum to string conversion") {
    // TODO! By the way I don't think the enum string conversions should be in
    // the defs.hpp file but let's use this to show a test
    REQUIRE(ToString(aare::DetectorType::Generic) == "Generic");
    REQUIRE(ToString(aare::DetectorType::Eiger) == "Eiger");
    REQUIRE(ToString(aare::DetectorType::Gotthard) == "Gotthard");
    REQUIRE(ToString(aare::DetectorType::Jungfrau) == "Jungfrau");
    REQUIRE(ToString(aare::DetectorType::ChipTestBoard) == "ChipTestBoard");
    REQUIRE(ToString(aare::DetectorType::Moench) == "Moench");
    REQUIRE(ToString(aare::DetectorType::Mythen3) == "Mythen3");
    REQUIRE(ToString(aare::DetectorType::Gotthard2) == "Gotthard2");
    REQUIRE(ToString(aare::DetectorType::Xilinx_ChipTestBoard) ==
            "Xilinx_ChipTestBoard");
    REQUIRE(ToString(aare::DetectorType::Moench03) == "Moench03");
    REQUIRE(ToString(aare::DetectorType::Moench03_old) == "Moench03_old");
    REQUIRE(ToString(aare::DetectorType::Unknown) == "Unknown");
}

TEST_CASE("String to enum") {
    REQUIRE(StringTo<aare::DetectorType>("Generic") ==
            aare::DetectorType::Generic);
    REQUIRE(StringTo<aare::DetectorType>("Eiger") == aare::DetectorType::Eiger);
    REQUIRE(StringTo<aare::DetectorType>("Gotthard") ==
            aare::DetectorType::Gotthard);
    REQUIRE(StringTo<aare::DetectorType>("Jungfrau") ==
            aare::DetectorType::Jungfrau);
    REQUIRE(StringTo<aare::DetectorType>("ChipTestBoard") ==
            aare::DetectorType::ChipTestBoard);
    REQUIRE(StringTo<aare::DetectorType>("Moench") ==
            aare::DetectorType::Moench);
    REQUIRE(StringTo<aare::DetectorType>("Mythen3") ==
            aare::DetectorType::Mythen3);
    REQUIRE(StringTo<aare::DetectorType>("Gotthard2") ==
            aare::DetectorType::Gotthard2);
    REQUIRE(StringTo<aare::DetectorType>("Xilinx_ChipTestBoard") ==
            aare::DetectorType::Xilinx_ChipTestBoard);
    REQUIRE(StringTo<aare::DetectorType>("Moench03") ==
            aare::DetectorType::Moench03);
    REQUIRE(StringTo<aare::DetectorType>("Moench03_old") ==
            aare::DetectorType::Moench03_old);
    REQUIRE(StringTo<aare::DetectorType>("Unknown") ==
            aare::DetectorType::Unknown);
}

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