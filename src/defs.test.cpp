// SPDX-License-Identifier: MPL-2.0
#include "aare/defs.hpp"

#include <catch2/catch_test_macros.hpp>
#include <string>

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

TEST_CASE("Basic ops on BitOffset") {
    REQUIRE_THROWS(aare::BitOffset(10));

    aare::BitOffset offset(5);
    REQUIRE(offset.value() == 5);

    aare::BitOffset offset2;
    REQUIRE(offset2.value() == 0);

    aare::BitOffset offset3(offset);
    REQUIRE(offset3.value() == 5);

    REQUIRE(offset == offset3);

    // Now assign offset to offset2 which should get the value 5
    offset2 = offset;
    REQUIRE(offset2.value() == 5);
    REQUIRE(offset2 == offset);
}
