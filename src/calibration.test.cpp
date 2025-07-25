/************************************************
 * @file test-Cluster.cpp
 * @short test case for generic Cluster, ClusterVector, and calculate_eta2
 ***********************************************/

#include "aare/calibration.hpp"

// #include "catch.hpp"
#include <array>
#include <catch2/catch_all.hpp>
#include <catch2/catch_test_macros.hpp>

using namespace aare;

TEST_CASE("Test Pedestal Generation", "[.calibration]") {
    NDArray<uint16_t, 3> raw(std::array<ssize_t, 3>{3, 2, 2}, 0);

    // gain 0
    raw(0, 0, 0) = 100;
    raw(1, 0, 0) = 200;
    raw(2, 0, 0) = 300;

    // gain 1
    raw(0, 0, 1) = (1 << 14) + 100;
    raw(1, 0, 1) = (1 << 14) + 200;
    raw(2, 0, 1) = (1 << 14) + 300;

    raw(0, 1, 0) = (1 << 14) + 37;
    raw(1, 1, 0) = 38;
    raw(2, 1, 0) = (3 << 14) + 39;

    // gain 2
    raw(0, 1, 1) = (3 << 14) + 100;
    raw(1, 1, 1) = (3 << 14) + 200;
    raw(2, 1, 1) = (3 << 14) + 300;

    auto pedestal = calculate_pedestal<double>(raw.view(), 4);

    REQUIRE(pedestal.size() == raw.size());
    CHECK(pedestal(0, 0, 0) == 200);
    CHECK(pedestal(1, 0, 0) == 0);
    CHECK(pedestal(1, 0, 1) == 200);

    auto pedestal_gain0 = calculate_pedestal_g0<double>(raw.view(), 4);

    REQUIRE(pedestal_gain0.size() == 4);
    CHECK(pedestal_gain0(0, 0) == 200);
    CHECK(pedestal_gain0(1, 0) == 38);
}