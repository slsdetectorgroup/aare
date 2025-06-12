#include "aare/scan_parameters.hpp"

#include <catch2/catch_test_macros.hpp>

using namespace aare;

TEST_CASE("Parse scan parameters") {
    ScanParameters s("[enabled\ndac dac 4\nstart 500\nstop 2200\nstep "
                     "5\nsettleTime 100us\n]");
    REQUIRE(s.enabled());
    REQUIRE(s.dac() == "dac 4");
    REQUIRE(s.start() == 500);
    REQUIRE(s.stop() == 2200);
    REQUIRE(s.step() == 5);
}

TEST_CASE("A disabled scan") {
    ScanParameters s("[disabled]");
    REQUIRE_FALSE(s.enabled());
    REQUIRE(s.dac() == "");
    REQUIRE(s.start() == 0);
    REQUIRE(s.stop() == 0);
    REQUIRE(s.step() == 0);
}
