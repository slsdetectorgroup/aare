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

TEST_CASE("Correct Instantiation of Cluster and ClusterVector",
          "[.cluster][.instantiation]") {

    CHECK(is_valid_cluster<double, 3, 3>);
    CHECK(is_valid_cluster<double, 3, 2>);
    CHECK(not is_valid_cluster<int, 0, 0>);
    CHECK(not is_valid_cluster<std::string, 2, 2>);
    CHECK(not is_valid_cluster<int, 2, 2, double>);

    CHECK(not is_cluster_v<int>);
    CHECK(is_cluster_v<Cluster<int, 3, 3>>);
}
<<<<<<< Updated upstream
=======

using ClusterTypes =
    std::variant<Cluster<int, 2, 2>, Cluster<int, 3, 3>, Cluster<int, 5, 5>,
                 Cluster<int, 4, 2>, Cluster<int, 2, 3>>;

auto get_test_sum_parameters() {
    return GENERATE(
        std::make_tuple(ClusterTypes{Cluster<int, 2, 2>{0, 0, {1, 2, 3, 1}}},
                        std::make_pair(7, 0)),
        std::make_tuple(
            ClusterTypes{Cluster<int, 3, 3>{0, 0, {1, 2, 3, 4, 5, 6, 1, 2, 7}}},
            std::make_pair(20, 3)),
        std::make_tuple(ClusterTypes{Cluster<int, 5, 5>{
                            0, 0, {1, 6, 7, 6, 5, 4, 3, 2, 1, 8, 8, 9, 2,
                                   1, 4, 5, 6, 7, 8, 4, 1, 1, 1, 1, 1}}},
                        std::make_pair(28, 8)),
        std::make_tuple(
            ClusterTypes{Cluster<int, 4, 2>{0, 0, {1, 4, 7, 2, 5, 6, 4, 3}}},
            std::make_pair(21, 1)),
        std::make_tuple(
            ClusterTypes{Cluster<int, 2, 3>{0, 0, {1, 3, 2, 3, 4, 2}}},
            std::make_pair(11, 1)));
}

TEST_CASE("compute_largest_2x2_subcluster", "[.cluster]") {
    auto [cluster, sum_pair] = get_test_sum_parameters();

    auto sum = std::visit(
        [](const auto &clustertype) { return clustertype.max_sum_2x2(); },
        cluster);
    CHECK(sum_pair.first == sum.first);
    CHECK(sum_pair.second == sum.second);
}

TEST_CASE("Test sum of Cluster", "[.cluster]") {
    Cluster<int, 2, 2> cluster{0, 0, {1, 2, 3, 4}};

    CHECK(cluster.sum() == 10);

    Cluster<int, 2, 3> cluster2x3{0, 0, {1, 3, 2, 3, 4, 2}};

    CHECK(cluster2x3.sum() == 15);
}
>>>>>>> Stashed changes
