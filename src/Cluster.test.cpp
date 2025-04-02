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
