#include "aare/processing/Pedestal.hpp"
#include "aare/utils/floats.hpp"
#include <catch2/catch_test_macros.hpp>
#include <chrono>
#include <random>

using namespace aare;
TEST_CASE("test pedestal constructor") {
    aare::Pedestal pedestal(10, 10, 5);
    REQUIRE(pedestal.rows() == 10);
    REQUIRE(pedestal.cols() == 10);
    REQUIRE(pedestal.n_samples() == 5);
    REQUIRE(pedestal.cur_samples() != nullptr);
    REQUIRE(pedestal.get_sum() != nullptr);
    REQUIRE(pedestal.get_sum2() != nullptr);
    for (int i = 0; i < 10; i++) {
        for (int j = 0; j < 10; j++) {
            REQUIRE(pedestal.get_sum()[pedestal.index(i, j)] == 0);
            REQUIRE(pedestal.get_sum2()[pedestal.index(i, j)] == 0);
            REQUIRE(pedestal.cur_samples()[pedestal.index(i, j)] == 0);
        }
    }
}

TEST_CASE("test pedestal push") {
    aare::Pedestal pedestal(10, 10, 5);
    aare::Frame frame(10, 10, 16);
    for (int i = 0; i < 10; i++) {
        for (int j = 0; j < 10; j++) {
            frame.set<uint16_t>(i, j, i + j);
        }
    }

    // test single push
    pedestal.push<uint16_t>(frame);
    for (int i = 0; i < 10; i++) {
        for (int j = 0; j < 10; j++) {
            REQUIRE(pedestal.get_sum()[pedestal.index(i, j)] == i + j);
            REQUIRE(pedestal.get_sum2()[pedestal.index(i, j)] == (i + j) * (i + j));
            REQUIRE(pedestal.cur_samples()[pedestal.index(i, j)] == 1);
        }
    }

    // test clear
    pedestal.clear();
    for (int i = 0; i < 10; i++) {
        for (int j = 0; j < 10; j++) {
            REQUIRE(pedestal.get_sum()[pedestal.index(i, j)] == 0);
            REQUIRE(pedestal.get_sum2()[pedestal.index(i, j)] == 0);
            REQUIRE(pedestal.cur_samples()[pedestal.index(i, j)] == 0);
        }
    }

    // test number of samples after multiple push
    for (int k = 0; k < 50; k++) {
        pedestal.push<uint16_t>(frame);
        for (int i = 0; i < 10; i++) {
            for (int j = 0; j < 10; j++) {
                if (k < 5) {
                    REQUIRE(pedestal.cur_samples()[pedestal.index(i, j)] == k + 1);
                    REQUIRE(pedestal.get_sum()[pedestal.index(i, j)] == (k + 1) * (i + j));
                    REQUIRE(pedestal.get_sum2()[pedestal.index(i, j)] == (k + 1) * (i + j) * (i + j));
                } else {
                    REQUIRE(pedestal.cur_samples()[pedestal.index(i, j)] == 5);
                    REQUIRE(pedestal.get_sum()[pedestal.index(i, j)] == 5 * (i + j));
                    REQUIRE(pedestal.get_sum2()[pedestal.index(i, j)] == 5 * (i + j) * (i + j));
                }
                REQUIRE(pedestal.mean(i, j) == (i + j));
                REQUIRE(pedestal.variance(i, j) == 0);
                REQUIRE(pedestal.standard_deviation(i, j) == 0);
            }
        }
    }
}

TEST_CASE("test pedestal with normal distribution") {
    const double MEAN = 5.0, STD = 2.0, VAR = STD * STD, TOLERANCE = 0.1;

    unsigned seed = std::chrono::system_clock::now().time_since_epoch().count();
    std::default_random_engine generator(seed);
    std::normal_distribution<double> distribution(MEAN, STD);

    aare::Pedestal pedestal(4, 4, 10000);
    for (int i = 0; i < 10000; i++) {
        aare::Frame frame(4, 4, 64);
        for (int j = 0; j < 4; j++) {
            for (int k = 0; k < 4; k++) {
                frame.set<double>(j, k, distribution(generator));
            }
        }
        pedestal.push<double>(frame);
    }

    auto mean = pedestal.mean();
    auto variance = pedestal.variance();
    auto standard_deviation = pedestal.standard_deviation();

    for (int i = 0; i < 4; i++) {
        for (int j = 0; j < 4; j++) {
            // 10% tolerance
            REQUIRE(compare_floats<double>(mean(i, j), MEAN, MEAN * TOLERANCE));
            REQUIRE(compare_floats<double>(variance(i, j), VAR, VAR * TOLERANCE));
            REQUIRE(compare_floats<double>(standard_deviation(i, j), STD, STD * TOLERANCE)); // maybe sqrt of tolerance?
        }
    }
}