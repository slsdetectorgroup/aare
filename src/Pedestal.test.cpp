#include "aare/Pedestal.hpp"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <chrono>
#include <random>

using namespace aare;
TEST_CASE("test pedestal constructor") {
    aare::Pedestal pedestal(10, 10, 5);
    REQUIRE(pedestal.rows() == 10);
    REQUIRE(pedestal.cols() == 10);
    REQUIRE(pedestal.n_samples() == 5);
    for (int i = 0; i < 10; i++) {
        for (int j = 0; j < 10; j++) {
            REQUIRE(pedestal.get_sum()(i, j) == 0);
            REQUIRE(pedestal.get_sum2()(i, j) == 0);
            REQUIRE(pedestal.cur_samples()(i, j) == 0);
        }
    }
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
            REQUIRE(pedestal.get_sum()(i, j) == i + j);
            REQUIRE(pedestal.get_sum2()(i, j) == (i + j) * (i + j));
            REQUIRE(pedestal.cur_samples()(i, j) == 1);
        }
    }

    // test clear
    pedestal.clear();
    for (int i = 0; i < 10; i++) {
        for (int j = 0; j < 10; j++) {
            REQUIRE(pedestal.get_sum()(i, j) == 0);
            REQUIRE(pedestal.get_sum2()(i, j) == 0);
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
                    REQUIRE(pedestal.get_sum()(i, j) == (k + 1) * (i + j));
                    REQUIRE(pedestal.get_sum2()(i, j) ==
                            (k + 1) * (i + j) * (i + j));
                } else {
                    REQUIRE(pedestal.cur_samples()(i, j) == 5);
                    REQUIRE(pedestal.get_sum()(i, j) == 5 * (i + j));
                    REQUIRE(pedestal.get_sum2()(i, j) == 5 * (i + j) * (i + j));
                }
                REQUIRE(pedestal.mean(i, j) == (i + j));
                REQUIRE(pedestal.variance(i, j) == 0);
                REQUIRE(pedestal.std(i, j) == 0);
            }
        }
    }
}

TEST_CASE("test pedestal with normal distribution") {
    const double MEAN = 5.0, STD = 2.0, VAR = STD * STD, TOLERANCE = 0.1;

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
    auto variance = pedestal.variance();
    auto standard_deviation = pedestal.std();

    for (int i = 0; i < 3; i++) {
        for (int j = 0; j < 5; j++) {
            REQUIRE_THAT(mean(i, j),
                         Catch::Matchers::WithinAbs(MEAN, MEAN * TOLERANCE));
            REQUIRE_THAT(variance(i, j),
                         Catch::Matchers::WithinAbs(VAR, VAR * TOLERANCE));
            REQUIRE_THAT(standard_deviation(i, j),
                         Catch::Matchers::WithinAbs(STD, STD * TOLERANCE));
        }
    }
}