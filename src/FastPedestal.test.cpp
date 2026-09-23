// SPDX-License-Identifier: MPL-2.0
#include "aare/FastPedestal.hpp"

#include <catch2/catch_template_test_macros.hpp>
#include <catch2/catch_test_macros.hpp>

using namespace aare;

TEMPLATE_TEST_CASE("fast pedestal keeps variance in double precision",
                   "[fast-pedestal]", double, float, int16_t) {
    FastPedestal<TestType> pedestal(1, 1, 2);
    NDArray<uint16_t, 2> frame({1, 1}, 0);
    pedestal.add_init_frame(frame.view());
    frame[0] = 1000;
    pedestal.add_init_frame(frame.view());

    const auto &ready = pedestal;
    REQUIRE(ready.std(0, 0) == static_cast<TestType>(500));
    REQUIRE(ready.std_unchecked(0) == ready.std(0, 0));
    REQUIRE(ready.std()[0] == ready.std(0, 0));

    pedestal.push_ema(frame.view());
    REQUIRE(ready.std(0, 0) == static_cast<TestType>(std::sqrt(187500.0)));
    REQUIRE(ready.std_unchecked(0) == ready.std(0, 0));
    REQUIRE(ready.std()[0] == ready.std(0, 0));
}

TEST_CASE("fast pedestal validates standard deviation access",
          "[fast-pedestal]") {
    FastPedestal<double> pedestal(1, 1, 1);
    REQUIRE_THROWS(pedestal.std());
    REQUIRE_THROWS(pedestal.std(0, 0));

    NDArray<uint16_t, 2> frame({1, 1}, 42);
    pedestal.add_init_frame(frame.view());
    REQUIRE(pedestal.std(0, 0) == 0.0);
    REQUIRE_THROWS(pedestal.std(1, 0));
    REQUIRE_THROWS(pedestal.std(0, 1));
}

TEMPLATE_TEST_CASE("fast pedestal std stays finite after settling",
                   "[fast-pedestal]", double, float, int16_t) {
    FastPedestal<TestType> pedestal(1, 1, 10);
    NDArray<uint16_t, 2> frame({1, 1}, 16382);
    pedestal.add_init_frame(frame.view());
    frame[0] = 16383;
    for (int i = 0; i < 9; ++i) {
        pedestal.add_init_frame(frame.view());
    }
    for (int i = 0; i < 300; ++i) {
        pedestal.push_ema(frame.view());
    }

    const auto noise = pedestal.std(0, 0);
    REQUIRE(std::isfinite(noise));
    REQUIRE(noise >= 0);
    REQUIRE(static_cast<double>(noise) < 1e-3);
    REQUIRE(pedestal.std_unchecked(0) == noise);
    REQUIRE(pedestal.std()[0] == noise);
}
