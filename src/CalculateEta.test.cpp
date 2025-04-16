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
                        Eta2<int>{2. / 3, 3. / 4, corner::cBottomLeft, 7}),
        std::make_tuple(
            ClusterTypes{Cluster<int, 3, 3>{0, 0, {1, 2, 3, 4, 5, 6, 1, 2, 7}}},
            Eta2<int>{6. / 11, 2. / 7, corner::cTopRight, 20}),
        std::make_tuple(ClusterTypes{Cluster<int, 5, 5>{
                            0, 0, {1, 6, 7, 6, 5, 4, 3, 2, 1, 8, 8, 9, 2,
                                   1, 4, 5, 6, 7, 8, 4, 1, 1, 1, 1, 1}}},
                        Eta2<int>{9. / 17, 5. / 13, 8, 28}),
        std::make_tuple(
            ClusterTypes{Cluster<int, 4, 2>{0, 0, {1, 4, 7, 2, 5, 6, 4, 3}}},
            Eta2<int>{7. / 11, 6. / 10, 1, 21}),
        std::make_tuple(
            ClusterTypes{Cluster<int, 2, 3>{0, 0, {1, 3, 2, 3, 4, 2}}},
            Eta2<int>{3. / 5, 4. / 6, 1, 11}));
}

TEST_CASE("calculate_eta2", "[.eta_calculation]") {

    auto [cluster, expected_eta] = get_test_parameters();

    auto eta = std::visit(
        [](const auto &clustertype) { return calculate_eta2(clustertype); },
        cluster);

    CHECK(eta.x == expected_eta.x);
    CHECK(eta.y == expected_eta.y);
    CHECK(eta.c == expected_eta.c);
    CHECK(eta.sum == expected_eta.sum);
}
