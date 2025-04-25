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