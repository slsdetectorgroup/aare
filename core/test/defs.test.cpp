#include "aare/defs.hpp"
#include <catch2/catch_test_macros.hpp>
#include <string>

TEST_CASE("testing DetectorType") {
    SECTION("Enum to string conversion") {
        // By the way I don't think the enum string conversions should be in the defs.hpp file
        // but let's use this to show a test
        REQUIRE(toString(DetectorType::Jungfrau) == "Jungfrau");
        REQUIRE(toString(DetectorType::Moench) == "Moench");
        REQUIRE(toString(DetectorType::Mythen3) == "Mythen3");
        REQUIRE(toString(DetectorType::Eiger) == "Eiger");
    }

    SECTION("string to Enum conversion") {
        // By the way I don't think the enum string conversions should be in the defs.hpp file
        // but let's use this to show a test
        REQUIRE(StringTo<DetectorType>("Jungfrau") == DetectorType::Jungfrau);
        REQUIRE(StringTo<DetectorType>("Moench") == DetectorType::Moench);
        REQUIRE(StringTo<DetectorType>("Mythen3") == DetectorType::Mythen3);
        REQUIRE(StringTo<DetectorType>("Eiger") == DetectorType::Eiger);
    }
}

TEST_CASE("testing TimingMode") {
    SECTION("Enum to string conversion") {
        REQUIRE(toString(TimingMode::Auto) == "auto");
        REQUIRE(toString(TimingMode::Trigger) == "trigger");
    }

    SECTION("string to Enum conversion") {
        REQUIRE(StringTo<TimingMode>("auto") == TimingMode::Auto);
        REQUIRE(StringTo<TimingMode>("trigger") == TimingMode::Trigger);
    }
}