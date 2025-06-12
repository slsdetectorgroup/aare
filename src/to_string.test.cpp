#include "aare/to_string.hpp"

#include <catch2/catch_test_macros.hpp>

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
