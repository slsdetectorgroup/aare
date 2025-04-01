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

using ClusterTypes =
    std::variant<Cluster<int, 2, 2>, Cluster<int, 3, 3>, Cluster<int, 5, 5>,
                 Cluster<int, 4, 2>, Cluster<int, 2, 3>>;

TEST_CASE("calculate_eta2", "[.cluster][.eta_calculation]") {

    // weird expect cluster_start to be in bottom_left corner -> row major ->
    // check how its used should be an image!!

    auto [cluster, expected_eta] = GENERATE(
        std::make_tuple(ClusterTypes{Cluster<int, 2, 2>{0, 0, {1, 2, 3, 1}}},
                        Eta2{2. / 3, 3. / 4, corner::cBottomLeft, 7}),
        std::make_tuple(
            ClusterTypes{Cluster<int, 3, 3>{0, 0, {1, 2, 3, 4, 5, 6, 1, 2, 7}}},
            Eta2{6. / 11, 2. / 7, corner::cTopRight, 20}),
        std::make_tuple(ClusterTypes{Cluster<int, 5, 5>{
                            0, 0, {1, 6, 7, 6, 5, 4, 3, 2, 1, 8, 8, 9, 2,
                                   1, 4, 5, 6, 7, 8, 4, 1, 1, 1, 1, 1}}},
                        Eta2{9. / 17, 5. / 13, 8, 28}),
        std::make_tuple(
            ClusterTypes{Cluster<int, 4, 2>{0, 0, {1, 4, 7, 2, 5, 6, 4, 3}}},
            Eta2{7. / 11, 6. / 10, 1, 21}),
        std::make_tuple(
            ClusterTypes{Cluster<int, 2, 3>{0, 0, {1, 3, 2, 3, 4, 2}}},
            Eta2{3. / 5, 4. / 6, 1, 11}));

    Eta2 eta = std::visit(
        [](const auto &clustertype) { return calculate_eta2(clustertype); },
        cluster);

    CHECK(eta.x == expected_eta.x);
    CHECK(eta.y == expected_eta.y);
    CHECK(eta.c == expected_eta.c);
    CHECK(eta.sum == expected_eta.sum);
}
