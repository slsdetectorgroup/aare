/************************************************
 * @file CalculateEta.test.cpp
 * @short test case to calculate_eta2
 ***********************************************/

#include "aare/CalculateEta.hpp"
#include "aare/Cluster.hpp"
#include "aare/ClusterFile.hpp"

// #include "catch.hpp"
#include <array>
#include <catch2/catch_all.hpp>
#include <catch2/catch_test_macros.hpp>

using namespace aare;

using ClusterTypes =
    std::variant<Cluster<int, 2, 2>, Cluster<int, 3, 3>, Cluster<int, 5, 5>,
                 Cluster<int, 4, 2>, Cluster<int, 2, 3>>;

auto get_test_parameters() {
    return GENERATE(
        std::make_tuple(ClusterTypes{Cluster<int, 2, 2>{0, 0, {1, 2, 3, 1}}},
                        Eta2<int>{1. / 4, 1. / 3, corner::cTopLeft, 7}),
        std::make_tuple(
            ClusterTypes{Cluster<int, 3, 3>{0, 0, {1, 2, 3, 4, 5, 6, 1, 2, 7}}},
            Eta2<int>{6. / 11, 2. / 7, corner::cBottomRight, 20}),
        std::make_tuple(ClusterTypes{Cluster<int, 5, 5>{
                            0, 0, {1, 1, 1, 1, 1, 1, 1, 2, 1, 2, 1, 9, 8,
                                   1, 4, 1, 6, 7, 8, 1, 1, 1, 1, 1, 1}}},
                        Eta2<int>{8. / 17, 7. / 15, corner::cBottomLeft, 30}),
        std::make_tuple(
            ClusterTypes{Cluster<int, 4, 2>{0, 0, {1, 4, 7, 2, 5, 6, 4, 3}}},
            Eta2<int>{4. / 10, 4. / 11, corner::cTopLeft, 21}),
        std::make_tuple(
            ClusterTypes{Cluster<int, 2, 3>{0, 0, {1, 3, 2, 3, 4, 2}}},
            Eta2<int>{3. / 5, 2. / 5, corner::cBottomLeft, 11}));
}

TEST_CASE("compute_largest_2x2_subcluster", "[eta_calculation]") {
    auto [cluster, expected_eta] = get_test_parameters();

    auto [sum, index] = std::visit(
        [](const auto &clustertype) { return clustertype.max_sum_2x2(); },
        cluster);
    CHECK(expected_eta.c == index);
    CHECK(expected_eta.sum == sum);
}

TEST_CASE("calculate_eta2", "[eta_calculation]") {

    auto [cluster, expected_eta] = get_test_parameters();

    auto eta = std::visit(
        [](const auto &clustertype) { return calculate_eta2(clustertype); },
        cluster);

    CHECK(eta.x == expected_eta.x);
    CHECK(eta.y == expected_eta.y);
    CHECK(eta.c == expected_eta.c);
    CHECK(eta.sum == expected_eta.sum);
}

// 3x3 cluster layout (rotated to match the cBottomLeft enum):
//  6, 7, 8
//  3, 4, 5
//  0, 1, 2

TEST_CASE("Calculate eta2 for a 3x3 int32 cluster with the largest 2x2 sum in "
          "the bottom left",
          "[eta_calculation]") {

    // Create a 3x3 cluster
    Cluster<int32_t, 3, 3> cl;
    cl.x = 0;
    cl.y = 0;
    cl.data[0] = 8;
    cl.data[1] = 2;
    cl.data[2] = 5;
    cl.data[3] = 20;
    cl.data[4] = 50;
    cl.data[5] = 3;
    cl.data[6] = 30;
    cl.data[7] = 23;
    cl.data[8] = 3;

    auto eta = calculate_eta2(cl);
    CHECK(eta.c == corner::cBottomLeft);
    CHECK(eta.x == 50.0 / (20 + 50));
    CHECK(eta.y == 23.0 / (23 + 50));
    CHECK(eta.sum == 30 + 23 + 20 + 50);
}

TEST_CASE("Calculate eta2 for a 3x3 int32 cluster with the largest 2x2 sum in "
          "the top right",
          "[eta_calculation]") {

    // Create a 3x3 cluster
    Cluster<int32_t, 3, 3> cl;
    cl.x = 0;
    cl.y = 0;
    cl.data[0] = 8;
    cl.data[1] = 12;
    cl.data[2] = 82;
    cl.data[3] = 77;
    cl.data[4] = 80;
    cl.data[5] = 91;
    cl.data[6] = 5;
    cl.data[7] = 3;
    cl.data[8] = 3;

    auto eta = calculate_eta2(cl);
    CHECK(eta.c == corner::cTopRight);
    CHECK(eta.x == 91. / (80 + 91));
    CHECK(eta.y == 80.0 / (80 + 12));
    CHECK(eta.sum == 12 + 80 + 82 + 91);
}

auto get_test_parameters_fulleta2x2() {
    return GENERATE(
        std::make_tuple(ClusterTypes{Cluster<int, 2, 2>{0, 0, {1, 2, 3, 1}}},
                        Eta2<int>{3. / 7, 4. / 7, corner::cTopLeft, 7}),
        std::make_tuple(
            ClusterTypes{Cluster<int, 3, 3>{0, 0, {1, 2, 3, 4, 5, 6, 1, 2, 7}}},
            Eta2<int>{13. / 20, 9. / 20, corner::cBottomRight, 20}),
        std::make_tuple(ClusterTypes{Cluster<int, 5, 5>{
                            0, 0, {1, 1, 1, 1, 1, 1, 1, 2, 1, 2, 1, 9, 8,
                                   1, 4, 1, 6, 7, 8, 1, 1, 1, 1, 1, 1}}},
                        Eta2<int>{15. / 30, 13. / 30, corner::cBottomLeft, 30}),
        std::make_tuple(
            ClusterTypes{Cluster<int, 4, 2>{0, 0, {1, 4, 7, 2, 5, 6, 4, 3}}},
            Eta2<int>{11. / 21, 10. / 21, corner::cTopLeft, 21}),
        std::make_tuple(
            ClusterTypes{Cluster<int, 2, 3>{0, 0, {1, 3, 2, 3, 4, 2}}},
            Eta2<int>{5. / 11, 6. / 11, corner::cBottomLeft, 11}));
}

TEST_CASE("Calculate full eta2", "[eta_calculation]") {

    auto [test_cluster, expected_eta] = get_test_parameters_fulleta2x2();

    auto eta = std::visit(
        [](const auto &clustertype) {
            return calculate_full_eta2(clustertype);
        },
        test_cluster);
    CHECK(expected_eta.c == eta.c);
    CHECK(expected_eta.sum == eta.sum);
    CHECK(expected_eta.x == eta.x);
    CHECK(expected_eta.y == eta.y);
}