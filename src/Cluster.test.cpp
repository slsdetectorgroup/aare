/************************************************
 * @file test-Cluster.cpp
 * @short test case for generic Cluster, ClusterVector, and calculate_eta2
 ***********************************************/

#include "aare/Cluster.hpp"
#include "aare/CalculateEta.hpp"
#include "aare/ClusterFile.hpp"

// #include "catch.hpp"
#include <array>
#include <catch2/catch_all.hpp>
#include <catch2/catch_test_macros.hpp>

using namespace aare;

TEST_CASE("Test sum of Cluster", "[.cluster]") {
    Cluster<int, 2, 2> cluster{0, 0, {1, 2, 3, 4}};

    CHECK(cluster.sum() == 10);
}

using ClusterTypes = std::variant<Cluster<int, 2, 2>, Cluster<int, 3, 3>,
                                  Cluster<int, 5, 5>, Cluster<int, 2, 3>>;

using ClusterTypesLargerThan2x2 =
    std::variant<Cluster<int, 3, 3>, Cluster<int, 4, 4>, Cluster<int, 5, 5>>;

TEST_CASE("Test reduce to 2x2 Cluster", "[.cluster]") {
    auto [cluster, expected_reduced_cluster] = GENERATE(
        std::make_tuple(ClusterTypes{Cluster<int, 2, 2>{5, 5, {1, 2, 3, 4}}},
                        Cluster<int, 2, 2>{4, 6, {1, 2, 3, 4}}),
        std::make_tuple(
            ClusterTypes{Cluster<int, 3, 3>{5, 5, {1, 1, 1, 1, 3, 2, 1, 2, 2}}},
            Cluster<int, 2, 2>{5, 5, {3, 2, 2, 2}}),
        std::make_tuple(
            ClusterTypes{Cluster<int, 3, 3>{5, 5, {1, 1, 1, 2, 3, 1, 2, 2, 1}}},
            Cluster<int, 2, 2>{4, 5, {2, 3, 2, 2}}),
        std::make_tuple(
            ClusterTypes{Cluster<int, 3, 3>{5, 5, {2, 2, 1, 2, 3, 1, 1, 1, 1}}},
            Cluster<int, 2, 2>{4, 6, {2, 2, 2, 3}}),
        std::make_tuple(
            ClusterTypes{Cluster<int, 3, 3>{5, 5, {1, 2, 2, 1, 3, 2, 1, 1, 1}}},
            Cluster<int, 2, 2>{5, 6, {2, 2, 3, 2}}),
        std::make_tuple(ClusterTypes{Cluster<int, 5, 5>{
                            5, 5, {1, 1, 1, 1, 1, 1, 1, 2, 2, 1, 1, 1, 3,
                                   2, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1}}},
                        Cluster<int, 2, 2>{5, 6, {2, 2, 3, 2}}),
        std::make_tuple(ClusterTypes{Cluster<int, 5, 5>{
                            5, 5, {1, 1, 1, 1, 1, 1, 2, 2, 1, 1, 1, 2, 3,
                                   1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1}}},
                        Cluster<int, 2, 2>{4, 6, {2, 2, 2, 3}}),
        std::make_tuple(
            ClusterTypes{Cluster<int, 2, 3>{5, 5, {2, 2, 3, 2, 1, 1}}},
            Cluster<int, 2, 2>{4, 6, {2, 2, 3, 2}}));

    auto reduced_cluster = std::visit(
        [](const auto &clustertype) { return reduce_to_2x2(clustertype); },
        cluster);

    CHECK(reduced_cluster.x == expected_reduced_cluster.x);
    CHECK(reduced_cluster.y == expected_reduced_cluster.y);
    CHECK(std::equal(reduced_cluster.data.begin(),
                     reduced_cluster.data.begin() + 4,
                     expected_reduced_cluster.data.begin()));
}

TEST_CASE("Test reduce to 3x3 Cluster", "[.cluster]") {
    auto [cluster, expected_reduced_cluster] = GENERATE(
        std::make_tuple(ClusterTypesLargerThan2x2{Cluster<int, 3, 3>{
                            5, 5, {1, 1, 1, 1, 3, 1, 1, 1, 1}}},
                        Cluster<int, 3, 3>{5, 5, {1, 1, 1, 1, 3, 1, 1, 1, 1}}),
        std::make_tuple(
            ClusterTypesLargerThan2x2{Cluster<int, 4, 4>{
                5, 5, {2, 2, 1, 1, 2, 2, 1, 1, 1, 1, 3, 1, 1, 1, 1, 1}}},
            Cluster<int, 3, 3>{4, 6, {2, 2, 1, 2, 2, 1, 1, 1, 3}}),
        std::make_tuple(
            ClusterTypesLargerThan2x2{Cluster<int, 4, 4>{
                5, 5, {1, 1, 2, 2, 1, 1, 2, 2, 1, 1, 3, 1, 1, 1, 1, 1}}},
            Cluster<int, 3, 3>{5, 6, {1, 2, 2, 1, 2, 2, 1, 3, 1}}),
        std::make_tuple(
            ClusterTypesLargerThan2x2{Cluster<int, 4, 4>{
                5, 5, {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 3, 2, 1, 1, 2, 2}}},
            Cluster<int, 3, 3>{5, 5, {1, 1, 1, 1, 3, 2, 1, 2, 2}}),
        std::make_tuple(
            ClusterTypesLargerThan2x2{Cluster<int, 4, 4>{
                5, 5, {1, 1, 1, 1, 1, 1, 1, 1, 2, 2, 3, 1, 2, 2, 1, 1}}},
            Cluster<int, 3, 3>{4, 5, {1, 1, 1, 2, 2, 3, 2, 2, 1}}),
        std::make_tuple(ClusterTypesLargerThan2x2{Cluster<int, 5, 5>{
                            5, 5, {1, 1, 1, 1, 1, 1, 2, 1, 1, 1, 2, 2, 3,
                                   1, 1, 1, 2, 1, 1, 1, 1, 1, 1, 1, 1}}},
                        Cluster<int, 3, 3>{4, 5, {1, 2, 1, 2, 2, 3, 1, 2, 1}}));

    auto reduced_cluster = std::visit(
        [](const auto &clustertype) { return reduce_to_3x3(clustertype); },
        cluster);

    CHECK(reduced_cluster.x == expected_reduced_cluster.x);
    CHECK(reduced_cluster.y == expected_reduced_cluster.y);
    CHECK(std::equal(reduced_cluster.data.begin(),
                     reduced_cluster.data.begin() + 9,
                     expected_reduced_cluster.data.begin()));
}