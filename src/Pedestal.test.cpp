// SPDX-License-Identifier: MPL-2.0
#include "aare/Pedestal.hpp"

#include <catch2/catch_template_test_macros.hpp>
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>
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
    // Static cast to double is needed to avoid a Catch2 warning about
    // float to double conversion on mac.
    REQUIRE_THAT(
        static_cast<double>(pedestal.std(0, 0)),
        Catch::Matchers::WithinAbs(static_cast<double>(static_cast<TestType>(
                                       std::sqrt(TestType(1.1875)))),
                                   static_cast<double>(TestType(1e-6))));
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

namespace {

// Pixel values around 1000 that differ from pixel to pixel and frame to frame
NDArray<uint16_t, 2> pedestal_test_frame(uint32_t frame_index) {
    NDArray<uint16_t, 2> pixels({3, 4});
    for (ssize_t i = 0; i < pixels.size(); ++i) {
        pixels[i] = static_cast<uint16_t>(
            1000 + (frame_index * 37 + static_cast<uint32_t>(i) * 11) % 97);
    }
    return pixels;
}

} // namespace

TEMPLATE_TEST_CASE("pedestal restored from a state continues identically",
                   "[pedestal]", double, float, int16_t) {
    // A threshold that only some pixels pass leaves the counts unequal
    NDArray<TestType, 2> threshold({3, 4}, TestType{40});
    auto push = [&](Pedestal<TestType> &pedestal, uint32_t frame_index) {
        auto pixels = pedestal_test_frame(frame_index);
        if (frame_index % 2 == 0) {
            pedestal.push(pixels.view());
        } else {
            pedestal.push_with_threshold(pixels.view(), threshold.view());
        }
    };

    // Stored empty, while initializing and after some pixels have settled
    for (const uint32_t frames_before : {0u, 3u, 9u}) {
        CAPTURE(frames_before);
        Pedestal<TestType> original(3, 4, 5);
        uint32_t frame_index = 0;
        for (; frame_index < frames_before; ++frame_index) {
            push(original, frame_index);
        }
        const auto state = original.state();
        REQUIRE(state.dtype == Dtype(typeid(TestType)));
        REQUIRE(state.n_samples == 5);

        Pedestal<TestType> restored(state);
        // A pedestal that holds other data, with another n_samples
        Pedestal<TestType> given(3, 4, 2);
        given.push(pedestal_test_frame(50).view());
        given.set_state(state);

        for (auto *pedestal : {&restored, &given}) {
            REQUIRE(pedestal->mean() == original.mean());
            REQUIRE(pedestal->n_samples() == 5);
        }
        for (uint32_t i = 0; i < 10; ++i, ++frame_index) {
            push(original, frame_index);
            push(restored, frame_index);
            push(given, frame_index);
        }
        for (auto *pedestal : {&restored, &given}) {
            REQUIRE(pedestal->cur_samples() == original.cur_samples());
            REQUIRE(pedestal->state().sum == original.state().sum);
            REQUIRE(pedestal->state().sum2 == original.state().sum2);
            REQUIRE(pedestal->mean() == original.mean());
            REQUIRE(pedestal->std() == original.std());
        }
    }
}

TEST_CASE("pedestal state does not depend on the pedestal type", "[pedestal]") {
    Pedestal<double> original(3, 4, 5);
    for (uint32_t i = 0; i < 8; ++i) {
        original.push(pedestal_test_frame(i).view());
    }
    Pedestal<float> restored(original.state());
    REQUIRE(restored.state().dtype == Dtype::FLOAT);
    for (uint32_t row = 0; row < 3; ++row) {
        for (uint32_t col = 0; col < 4; ++col) {
            REQUIRE(restored.mean(row, col) ==
                    static_cast<float>(original.mean(row, col)));
        }
    }
}

TEST_CASE("pedestal rejects an invalid state and stays unchanged",
          "[pedestal]") {
    using Catch::Matchers::ContainsSubstring;
    Pedestal<double> pedestal(3, 4, 5);
    pedestal.push(pedestal_test_frame(0).view());
    const auto valid = pedestal.state();
    const auto mean = pedestal.mean();

    auto other_shape = Pedestal<double>(4, 3, 5).state();
    REQUIRE_THROWS_WITH(pedestal.set_state(other_shape),
                        ContainsSubstring("expected (3, 4)"));
    auto uneven = valid;
    uneven.sum2 = NDArray<double, 2>({3, 5}, 0.0);
    REQUIRE_THROWS_WITH(pedestal.set_state(uneven),
                        ContainsSubstring("expected (3, 4)"));
    auto no_samples = valid;
    no_samples.n_samples = 0;
    REQUIRE_THROWS_WITH(pedestal.set_state(no_samples),
                        ContainsSubstring("n_samples=0 must be positive"));
    auto too_many = valid;
    too_many.cur_samples(2, 3) = 6;
    REQUIRE_THROWS_WITH(pedestal.set_state(too_many),
                        ContainsSubstring("at most n_samples"));
    REQUIRE_THROWS_AS(Pedestal<double>(PedestalState{}), std::runtime_error);

    REQUIRE(pedestal.n_samples() == 5);
    REQUIRE(pedestal.cur_samples() == valid.cur_samples);
    REQUIRE(pedestal.mean() == mean);
}
