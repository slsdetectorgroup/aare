#include "aare/ClusterVector.hpp"
#include "aare/Interpolator.hpp"
#include "aare/NDArray.hpp"

#include <array>
#include <catch2/catch_all.hpp>
#include <catch2/catch_test_macros.hpp>
#include <iostream>

using namespace aare;

TEST_CASE("Test new Interpolation API", "[Interpolation]") {

    NDArray<double, 1> energy_bins(std::array<ssize_t, 1>{2});
    NDArray<double, 1> etax_bins(std::array<ssize_t, 1>{4}, 0.0);
    NDArray<double, 1> etay_bins(std::array<ssize_t, 1>{4}, 0.0);
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