#include "aare/utils/floats.hpp"
#include <catch2/catch_test_macros.hpp>
#include <iostream>

using namespace aare;
TEST_CASE("compare floats") {
    REQUIRE(compare_floats(1.0, 1.0));
    REQUIRE(!compare_floats(1.01, 1.0));
    REQUIRE(!compare_floats(1.0000001, 1.0));
    REQUIRE(compare_floats(1.0, 1.1, 0.2));
    REQUIRE(!compare_floats(1.0, 1.1, 0.05));
    // test that no scaling happens when user defined maxRelDiff is used
    REQUIRE(!compare_floats(1.0, 1.1, 0.01));
    REQUIRE(!compare_floats(100000.0, 100000.1, 0.01));
    //
    REQUIRE(compare_floats(0.000000001, 0.000000001));
    REQUIRE(compare_floats(0.000000000000001, 0.000000000000001));
    REQUIRE(compare_floats(1e-21, 1e-21));
    REQUIRE(!compare_floats(1e-21, 1e-22));
    REQUIRE(compare_floats(std::numeric_limits<double>::min(), std::numeric_limits<double>::min()));
    REQUIRE(!compare_floats(std::numeric_limits<double>::min(), (double)std::numeric_limits<float>::min()));

    // test signed floats
    REQUIRE(!compare_floats(6542.0, -6542.0));
    REQUIRE(!compare_floats(0.0, -6542.0));
    REQUIRE(compare_floats(0.0, -0.0));
    REQUIRE(compare_floats(0.0, -0.0, 0.1));
    REQUIRE(compare_floats(-5800.0, -5800.0));
    // test double
    double a, b;
    a = 1.0;
    b = 1.0;
    REQUIRE(compare_floats(a, b));
}