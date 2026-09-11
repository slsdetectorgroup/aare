// SPDX-License-Identifier: MPL-2.0
#include "aare/Pedestal.hpp"

#include <catch2/catch_template_test_macros.hpp>
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <chrono>
#include <random>

using namespace aare;

TEMPLATE_TEST_CASE("pedestal uses double precision moments", "[pedestal]",
                   double, float, int16_t) {
    Pedestal<TestType> pedestal(1, 1, 2);
    pedestal.push(0, 0, uint16_t{30000});
    pedestal.push(0, 0, uint16_t{30003});

    REQUIRE(pedestal.mean(0, 0) == static_cast<TestType>(30001.5));
    REQUIRE(pedestal.std(0, 0) == static_cast<TestType>(1.5));

    pedestal.push(0, 0, uint16_t{30002});

    REQUIRE(pedestal.mean(0, 0) == static_cast<TestType>(30001.75));
    REQUIRE_THAT(static_cast<double>(pedestal.std(0, 0)),
                 Catch::Matchers::WithinAbs(
                     static_cast<TestType>(std::sqrt(1.1875)), 1e-6));
}

TEST_CASE("test pedestal constructor") {
    aare::Pedestal pedestal(10, 10, 5);
    REQUIRE(pedestal.rows() == 10);
    REQUIRE(pedestal.cols() == 10);
    REQUIRE(pedestal.n_samples() == 5);
    for (int i = 0; i < 10; i++) {
        for (int j = 0; j < 10; j++) {
            REQUIRE(pedestal.mean(i, j) == 0);
            REQUIRE(pedestal.cur_samples()(i, j) == 0);
        }
    }
}

TEST_CASE("pedestal rejects mismatched frame shapes", "[pedestal]") {
    Pedestal<> pedestal(2, 3);
    pedestal.push(0, 0, uint16_t{7});
    NDArray<double, 2> threshold({2, 3}, 10.0);

    for (const auto shape :
         {std::array<uint32_t, 2>{2, 2}, std::array<uint32_t, 2>{3, 2}}) {
        Frame frame(shape[0], shape[1], Dtype::UINT16);
        REQUIRE_THROWS_WITH(pedestal.push(frame.view<uint16_t>()),
                            "Frame shape does not match pedestal shape");
        REQUIRE_THROWS_WITH(pedestal.push_with_threshold(frame.view<uint16_t>(),
                                                         threshold.view()),
                            "Frame shape does not match pedestal shape");
        REQUIRE_THROWS_WITH(pedestal.push<uint16_t>(frame),
                            "Frame shape does not match pedestal shape");
        REQUIRE(pedestal.mean(0, 0) == 7);
        REQUIRE(pedestal.cur_samples()(0, 0) == 1);
    }
}

TEMPLATE_TEST_CASE("pedestal std stays finite after settling", "[pedestal]",
                   double, float, int16_t) {
    Pedestal<TestType> pedestal(1, 1, 10);
    pedestal.push(0, 0, uint16_t{16382});
    for (int i = 0; i < 201; ++i) {
        pedestal.push(0, 0, uint16_t{16383});
    }

    const auto noise = pedestal.std(0, 0);
    REQUIRE(std::isfinite(noise));
    REQUIRE(noise >= 0);
    REQUIRE(static_cast<double>(noise) < 1e-3);
    REQUIRE(pedestal.std()(0, 0) == noise);
}

TEST_CASE("test pedestal push") {
    aare::Pedestal pedestal(10, 10, 5);
    aare::Frame frame(10, 10, Dtype::UINT16);
    for (int i = 0; i < 10; i++) {
        for (int j = 0; j < 10; j++) {
            frame.set<uint16_t>(i, j, i + j);
        }
    }

    // test single push
    pedestal.push<uint16_t>(frame);
    for (int i = 0; i < 10; i++) {
        for (int j = 0; j < 10; j++) {
            REQUIRE(pedestal.mean(i, j) == i + j);
            REQUIRE(pedestal.cur_samples()(i, j) == 1);
        }
    }

    // test clear
    pedestal.clear();
    for (int i = 0; i < 10; i++) {
        for (int j = 0; j < 10; j++) {
            REQUIRE(pedestal.mean(i, j) == 0);
            REQUIRE(pedestal.cur_samples()(i, j) == 0);
        }
    }

    // test number of samples after multiple push
    for (uint32_t k = 0; k < 50; k++) {
        pedestal.push<uint16_t>(frame);
        for (uint32_t i = 0; i < 10; i++) {
            for (uint32_t j = 0; j < 10; j++) {
                if (k < 5) {
                    REQUIRE(pedestal.cur_samples()(i, j) == k + 1);
                } else {
                    REQUIRE(pedestal.cur_samples()(i, j) == 5);
                }
                REQUIRE(pedestal.mean(i, j) == (i + j));
                REQUIRE(pedestal.std(i, j) == 0);
            }
        }
    }
}

TEST_CASE("test pedestal with normal distribution") {
    const double MEAN = 5.0, STD = 2.0, TOLERANCE = 0.1;

    unsigned seed = std::chrono::system_clock::now().time_since_epoch().count();
    std::default_random_engine generator(seed);
    std::normal_distribution<double> distribution(MEAN, STD);

    aare::Pedestal pedestal(3, 5, 10000);
    for (int i = 0; i < 10000; i++) {
        aare::Frame frame(3, 5, Dtype::DOUBLE);
        for (int j = 0; j < 3; j++) {
            for (int k = 0; k < 5; k++) {
                frame.set<double>(j, k, distribution(generator));
            }
        }
        pedestal.push<double>(frame);
    }
    auto mean = pedestal.mean();
    auto standard_deviation = pedestal.std();

    for (int i = 0; i < 3; i++) {
        for (int j = 0; j < 5; j++) {
            REQUIRE_THAT(mean(i, j),
                         Catch::Matchers::WithinAbs(MEAN, MEAN * TOLERANCE));
            REQUIRE_THAT(standard_deviation(i, j),
                         Catch::Matchers::WithinAbs(STD, STD * TOLERANCE));
        }
    }
}
