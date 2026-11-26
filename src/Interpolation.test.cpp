// SPDX-License-Identifier: MPL-2.0
#include "aare/ClusterVector.hpp"
#include "aare/Interpolator.hpp"
#include "aare/NDArray.hpp"

#include <array>
#include <catch2/catch_all.hpp>
#include <catch2/catch_test_macros.hpp>
#include <iostream>

using namespace aare;

TEST_CASE("Test new Interpolation API", "[Interpolation]") {

    NDArray<double, 1> energy_bins(std::array<double, 2>{0.0, 100.0});
    NDArray<double, 1> etax_bins(std::array<double, 4>{0.0, 0.3, 0.6, 1.0});
    NDArray<double, 1> etay_bins(std::array<double, 4>{0.0, 0.3, 0.6, 1.0});
    NDArray<double, 3> eta_distribution(std::array<ssize_t, 3>{3, 3, 1}, 0.0);

    Interpolator interpolator(eta_distribution.view(), etax_bins.view(),
                              etay_bins.view(), energy_bins.view());

    ClusterVector<Cluster<double, 3, 3>> cluster_vec{};

    cluster_vec.push_back(Cluster<double, 3, 3>{
        2, 2, std::array<double, 9>{1, 2, 2, 1, 4, 1, 1, 2, 1}});

    auto photons =
        interpolator.interpolate<calculate_eta2<double, 3, 3>>(cluster_vec);

    CHECK(photons.size() == 1);
}

TEST_CASE("Test constructor", "[Interpolation]") {
    NDArray<double, 1> energy_bins(std::array<ssize_t, 1>{2});
    NDArray<double, 1> etax_bins(std::array<ssize_t, 1>{4}, 0.0);
    NDArray<double, 1> etay_bins(std::array<ssize_t, 1>{4}, 0.0);
    NDArray<double, 3> eta_distribution(std::array<ssize_t, 3>{3, 3, 1});

    std::iota(eta_distribution.begin(), eta_distribution.end(), 1.0);

    Interpolator interpolator(eta_distribution.view(), etax_bins.view(),
                              etay_bins.view(), energy_bins.view());

    auto ietax = interpolator.get_ietax();
    auto ietay = interpolator.get_ietay();

    CHECK(ietax.shape(0) == 3);
    CHECK(ietax.shape(1) == 3);
    CHECK(ietax.shape(2) == 1);

    CHECK(ietay.shape(0) == 3);
    CHECK(ietay.shape(1) == 3);
    CHECK(ietay.shape(2) == 1);

    std::array<double, 9> expected_ietax{
        0.0, 0.0, 0.0, 4.0 / 11.0, 5.0 / 13.0, 6.0 / 15.0, 1.0, 1.0, 1.0};

    std::array<double, 9> expected_ietay{
        0.0, 2.0 / 5.0, 1.0, 0.0, 5.0 / 11.0, 1.0, 0.0, 8.0 / 17.0, 1.0};

    for (ssize_t i = 0; i < ietax.shape(0); i++) {
        for (ssize_t j = 0; j < ietax.shape(1); j++) {
            CHECK(ietax(i, j, 0) ==
                  Catch::Approx(expected_ietax[i * ietax.shape(1) + j]));
        }
    }
    for (ssize_t i = 0; i < ietay.shape(0); i++) {
        for (ssize_t j = 0; j < ietay.shape(1); j++) {
            CHECK(ietay(i, j, 0) ==
                  Catch::Approx(expected_ietay[i * ietay.shape(1) + j]));
        }
    }
}

TEST_CASE("Test constructor with zero bins at borders", "[Interpolation]") {
    NDArray<double, 1> energy_bins(std::array<ssize_t, 1>{2});
    NDArray<double, 1> etax_bins(std::array<ssize_t, 1>{5}, 0.0);
    NDArray<double, 1> etay_bins(std::array<ssize_t, 1>{5}, 0.0);
    NDArray<double, 3> eta_distribution(std::array<ssize_t, 3>{4, 4, 1}, 0.0);

    eta_distribution(1, 1, 0) = 1.0;
    eta_distribution(1, 2, 0) = 2.0;
    eta_distribution(2, 1, 0) = 3.0;
    eta_distribution(2, 2, 0) = 4.0;

    Interpolator interpolator(eta_distribution.view(), etax_bins.view(),
                              etay_bins.view(), energy_bins.view());

    auto ietax = interpolator.get_ietax();
    auto ietay = interpolator.get_ietay();

    CHECK(ietax.shape(0) == 4);
    CHECK(ietax.shape(1) == 4);
    CHECK(ietax.shape(2) == 1);

    CHECK(ietay.shape(0) == 4);
    CHECK(ietay.shape(1) == 4);
    CHECK(ietay.shape(2) == 1);

    std::array<double, 16> expected_ietax{
        0.0, 0.0, 0.0, 0.0, 0.0, 1.0 / 4.0, 2.0 / 6.0, 0.0,
        0.0, 1.0, 1.0, 0.0, 0.0, 1.0,       1.0,       0.0};

    std::array<double, 16> expected_ietay{
        0.0, 0.0,       0.0, 0.0, 0.0, 1.0 / 3.0, 1.0, 1.0,
        0.0, 3.0 / 7.0, 1.0, 1.0, 0.0, 0.0,       0.0, 0.0};

    for (ssize_t i = 0; i < ietax.shape(0); i++) {
        for (ssize_t j = 0; j < ietax.shape(1); j++) {
            CHECK(ietax(i, j, 0) ==
                  Catch::Approx(expected_ietax[i * ietax.shape(1) + j]));
        }
    }
    for (ssize_t i = 0; i < ietay.shape(0); i++) {
        for (ssize_t j = 0; j < ietay.shape(1); j++) {
            CHECK(ietay(i, j, 0) ==
                  Catch::Approx(expected_ietay[i * ietay.shape(1) + j]));
        }
    }
}

TEST_CASE("Test Rosenblatt", "[Interpolation]") {
    NDArray<double, 1> energy_bins(std::array<ssize_t, 1>{2});
    NDArray<double, 1> etax_bins(std::array<ssize_t, 1>{4}, 0.0);
    NDArray<double, 1> etay_bins(std::array<ssize_t, 1>{4}, 0.0);
    NDArray<double, 3> eta_distribution(std::array<ssize_t, 3>{3, 3, 1});

    std::iota(eta_distribution.begin(), eta_distribution.end(), 1.0);

    Interpolator interpolator(etax_bins.view(), etay_bins.view(),
                              energy_bins.view());

    interpolator.rosenblatttransform(eta_distribution.view());

    auto ietax = interpolator.get_ietax();
    auto ietay = interpolator.get_ietay();

    CHECK(ietax.shape(0) == 3);
    CHECK(ietax.shape(1) == 3);
    CHECK(ietax.shape(2) == 1);

    CHECK(ietay.shape(0) == 3);
    CHECK(ietay.shape(1) == 3);
    CHECK(ietay.shape(2) == 1);

    // marginal CDF of eta_x
    std::array<double, 9> expected_ietax{
        0.0, 0.0, 0.0, 15.0 / 39.0, 15.0 / 39.0, 15.0 / 39.0, 1.0, 1.0, 1.0};

    // conditional CDF of eta_y
    std::array<double, 9> expected_ietay{
        0.0, 2.0 / 5.0, 1.0, 0.0, 5.0 / 11.0, 1.0, 0.0, 8.0 / 17.0, 1.0};

    for (ssize_t i = 0; i < ietax.shape(0); i++) {
        for (ssize_t j = 0; j < ietax.shape(1); j++) {
            CHECK(ietax(i, j, 0) ==
                  Catch::Approx(expected_ietax[i * ietax.shape(1) + j]));
        }
    }
    for (ssize_t i = 0; i < ietay.shape(0); i++) {
        for (ssize_t j = 0; j < ietay.shape(1); j++) {
            CHECK(ietay(i, j, 0) ==
                  Catch::Approx(expected_ietay[i * ietay.shape(1) + j]));
        }
    }
}

TEST_CASE("Interpolator restored from a state interpolates identically",
          "[Interpolation]") {
    NDArray<double, 1> energy_bins(std::array<double, 3>{0.0, 50.0, 100.0});
    NDArray<double, 1> etax_bins(std::array<double, 4>{0.0, 0.3, 0.6, 1.0});
    NDArray<double, 1> etay_bins(
        std::array<double, 5>{0.0, 0.2, 0.5, 0.7, 1.0});
    NDArray<double, 3> eta_distribution(std::array<ssize_t, 3>{3, 4, 2});
    std::iota(eta_distribution.begin(), eta_distribution.end(), 1.0);
    Interpolator original(eta_distribution.view(), etax_bins.view(),
                          etay_bins.view(), energy_bins.view());

    const auto state = original.state();
    // Parenthesised: Catch only prints two-dimensional arrays
    REQUIRE((state.etabinsy == etay_bins));
    REQUIRE((state.ietax == original.get_ietax()));
    REQUIRE((state.ietay == original.get_ietay()));

    Interpolator restored(state);
    // An interpolator with other bins and no distribution takes the state
    NDArray<double, 1> two_edges(std::array<double, 2>{0.0, 1.0});
    Interpolator given(two_edges.view(), two_edges.view(), two_edges.view());
    REQUIRE(given.state().ietax.size() == 0);
    given.set_state(state);

    ClusterVector<Cluster<double, 3, 3>> clusters{};
    clusters.push_back(Cluster<double, 3, 3>{
        2, 2, std::array<double, 9>{1, 2, 2, 1, 40, 1, 1, 20, 1}});
    clusters.push_back(Cluster<double, 3, 3>{
        7, 4, std::array<double, 9>{1, 2, 3, 30, 50, 1, 1, 2, 1}});
    const auto expected =
        original.interpolate<calculate_eta2<double, 3, 3>>(clusters);
    REQUIRE(expected.size() == 2);
    for (auto *interpolator : {&restored, &given}) {
        REQUIRE((interpolator->get_ietax() == original.get_ietax()));
        REQUIRE((interpolator->get_ietay() == original.get_ietay()));
        const auto photons =
            interpolator->interpolate<calculate_eta2<double, 3, 3>>(clusters);
        REQUIRE(photons.size() == expected.size());
        for (size_t i = 0; i < expected.size(); ++i) {
            REQUIRE(photons[i].x == expected[i].x);
            REQUIRE(photons[i].y == expected[i].y);
            REQUIRE(photons[i].energy == expected[i].energy);
        }
    }

    // Before a distribution has been given the state holds the bins only
    Interpolator bins_only(given.state().etabinsx.view(), etay_bins.view(),
                           energy_bins.view());
    REQUIRE(Interpolator(bins_only.state()).state().ietay.size() == 0);
}

TEST_CASE("Interpolator rejects an invalid state and stays unchanged",
          "[Interpolation]") {
    using Catch::Matchers::ContainsSubstring;
    NDArray<double, 1> energy_bins(std::array<double, 2>{0.0, 100.0});
    NDArray<double, 1> eta_bins(std::array<double, 4>{0.0, 0.3, 0.6, 1.0});
    NDArray<double, 3> eta_distribution(std::array<ssize_t, 3>{3, 3, 1}, 1.0);
    Interpolator interpolator(eta_distribution.view(), eta_bins.view(),
                              eta_bins.view(), energy_bins.view());
    const auto valid = interpolator.state();

    auto other_bins = valid;
    other_bins.etabinsx = NDArray<double, 1>(std::array<ssize_t, 1>{5}, 0.0);
    REQUIRE_THROWS_WITH(interpolator.set_state(other_bins),
                        ContainsSubstring("the shape (4, 3, 1)"));
    auto one_cdf = valid;
    one_cdf.ietay = NDArray<double, 3>{};
    REQUIRE_THROWS_WITH(interpolator.set_state(one_cdf),
                        ContainsSubstring("both must be empty"));
    auto one_edge = valid;
    one_edge.energy_bins = NDArray<double, 1>(std::array<ssize_t, 1>{1}, 0.0);
    REQUIRE_THROWS_WITH(interpolator.set_state(one_edge),
                        ContainsSubstring("every axis needs two edges"));
    REQUIRE_THROWS_AS(Interpolator(InterpolatorState{}), std::runtime_error);

    REQUIRE((interpolator.state().etabinsx == valid.etabinsx));
    REQUIRE((interpolator.get_ietax() == valid.ietax));
}
