// SPDX-License-Identifier: MPL-2.0
#include "aare/FastPedestal.hpp"

#include <catch2/catch_template_test_macros.hpp>
#include <catch2/catch_test_macros.hpp>

using namespace aare;

namespace {

// Deterministic frame with different values in every pixel and frame
NDArray<uint16_t, 2> pedestal_test_frame(ssize_t rows, ssize_t cols,
                                         uint32_t frame_index) {
    NDArray<uint16_t, 2> frame({rows, cols});
    uint32_t x = frame_index * 2654435761u + 12345u;
    for (ssize_t i = 0; i < frame.size(); ++i) {
        x = x * 1664525u + 1013904223u;
        frame[i] = static_cast<uint16_t>(1000 + (x >> 24));
    }
    return frame;
}

// Push as ClusterFinder does: initialize first, then update
template <typename T>
void push_pedestal_test_frame(FastPedestal<T> &pedestal, uint32_t frame_index) {
    auto frame =
        pedestal_test_frame(pedestal.rows(), pedestal.cols(), frame_index);
    if (pedestal.ready()) {
        pedestal.push_ema(frame.view());
    } else {
        pedestal.add_init_frame(frame.view());
    }
}

} // namespace

TEMPLATE_TEST_CASE("fast pedestal restored from a state continues identically",
                   "[fast-pedestal]", double, float, int16_t) {
    const uint32_t n_samples = 5;
    // Empty, initializing, just ready and updated after becoming ready
    for (const uint32_t frames_before : {0u, 3u, 5u, 12u}) {
        CAPTURE(frames_before);
        FastPedestal<TestType> original(3, 4, n_samples);
        uint32_t frame_index = 0;
        for (; frame_index < frames_before; ++frame_index) {
            push_pedestal_test_frame(original, frame_index);
        }

        const auto state = original.state();
        REQUIRE(state.dtype == Dtype(typeid(TestType)));
        REQUIRE(state.n_samples == n_samples);
        REQUIRE(state.cur_samples == std::min(frames_before, n_samples));
        REQUIRE(state.sum.shape() == Shape<2>{3, 4});
        REQUIRE(state.sum2.shape() == Shape<2>{3, 4});

        FastPedestal<TestType> constructed(state);
        // n_samples is replaced by that of the state
        FastPedestal<TestType> assigned(3, 4, 99);
        assigned.set_state(state);

        for (auto *restored : {&constructed, &assigned}) {
            REQUIRE(restored->rows() == 3u);
            REQUIRE(restored->cols() == 4u);
            REQUIRE(restored->n_samples() == n_samples);
            REQUIRE(restored->cur_samples() == original.cur_samples());
            REQUIRE(restored->ready() == original.ready());
            if (original.ready()) {
                REQUIRE(restored->mean() == original.mean());
                REQUIRE(restored->std() == original.std());
            }
        }

        // Enough frames to make every case ready and updated afterwards
        for (uint32_t i = 0; i < 10; ++i, ++frame_index) {
            push_pedestal_test_frame(original, frame_index);
            push_pedestal_test_frame(constructed, frame_index);
            push_pedestal_test_frame(assigned, frame_index);
        }

        const auto expected = original.state();
        for (auto *restored : {&constructed, &assigned}) {
            const auto actual = restored->state();
            REQUIRE(actual.n_samples == expected.n_samples);
            REQUIRE(actual.cur_samples == expected.cur_samples);
            REQUIRE(actual.sum == expected.sum);
            REQUIRE(actual.sum2 == expected.sum2);
            REQUIRE(restored->mean() == original.mean());
            REQUIRE(restored->std() == original.std());
        }
    }
}

TEST_CASE("fast pedestal state does not depend on the pedestal type",
          "[fast-pedestal]") {
    FastPedestal<double> source(2, 3, 4);
    for (uint32_t frame_index = 0; frame_index < 10; ++frame_index) {
        push_pedestal_test_frame(source, frame_index);
    }
    const auto state = source.state();
    REQUIRE(state.dtype == Dtype::DOUBLE);

    // The dtype of the state only records where it came from
    FastPedestal<float> as_float(state);
    FastPedestal<int16_t> as_int16(state);
    REQUIRE(as_float.state().dtype == Dtype::FLOAT);
    REQUIRE(as_int16.state().dtype == Dtype::INT16);

    const auto mean = source.mean();
    const auto mean_float = as_float.mean();
    const auto mean_int16 = as_int16.mean();
    for (ssize_t i = 0; i < mean.size(); ++i) {
        REQUIRE(mean_float[i] == static_cast<float>(mean[i]));
        REQUIRE(mean_int16[i] == static_cast<int16_t>(mean[i]));
    }

    // The moments pass through the other types unchanged, as does a state
    // without a dtype
    FastPedestal<float> from_untyped(2, 3, 4);
    auto untyped = state;
    untyped.dtype = Dtype::NONE;
    from_untyped.set_state(untyped);
    for (const auto &other :
         {as_float.state(), as_int16.state(), from_untyped.state()}) {
        REQUIRE(other.n_samples == state.n_samples);
        REQUIRE(other.cur_samples == state.cur_samples);
        REQUIRE(other.sum == state.sum);
        REQUIRE(other.sum2 == state.sum2);
    }
}

TEST_CASE("fast pedestal rejects an invalid state and stays unchanged",
          "[fast-pedestal]") {
    FastPedestal<double> pedestal(2, 3, 4);
    for (uint32_t frame_index = 0; frame_index < 6; ++frame_index) {
        push_pedestal_test_frame(pedestal, frame_index);
    }
    const auto before = pedestal.state();
    const auto mean_before = pedestal.mean();

    auto wrong_shape = before;
    wrong_shape.sum = NDArray<double, 2>({3, 2}, 0.0);
    wrong_shape.sum2 = NDArray<double, 2>({3, 2}, 0.0);
    REQUIRE_THROWS_AS(pedestal.set_state(wrong_shape), std::runtime_error);

    auto mismatched_moments = before;
    mismatched_moments.sum2 = NDArray<double, 2>({2, 4}, 0.0);
    REQUIRE_THROWS_AS(pedestal.set_state(mismatched_moments),
                      std::runtime_error);

    auto no_samples = before;
    no_samples.n_samples = 0;
    no_samples.cur_samples = 0;
    REQUIRE_THROWS_AS(pedestal.set_state(no_samples), std::runtime_error);

    auto too_many_samples = before;
    too_many_samples.cur_samples = before.n_samples + 1;
    REQUIRE_THROWS_AS(pedestal.set_state(too_many_samples), std::runtime_error);

    const auto after = pedestal.state();
    REQUIRE(after.n_samples == before.n_samples);
    REQUIRE(after.cur_samples == before.cur_samples);
    REQUIRE(after.sum == before.sum);
    REQUIRE(after.sum2 == before.sum2);
    REQUIRE(pedestal.mean() == mean_before);

    // An empty state has no shape to construct from
    REQUIRE_THROWS_AS(FastPedestal<double>(FastPedestalState{}),
                      std::runtime_error);
    REQUIRE_THROWS_AS(FastPedestal<double>(no_samples), std::runtime_error);
    REQUIRE_THROWS_AS(FastPedestal<double>(mismatched_moments),
                      std::runtime_error);
}

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
