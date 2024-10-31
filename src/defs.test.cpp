#include "aare/defs.hpp"
// #include "aare/utils/floats.hpp"

#include <catch2/catch_test_macros.hpp>
#include <string>
TEST_CASE("Enum to string conversion") {
    // By the way I don't think the enum string conversions should be in the defs.hpp file
    // but let's use this to show a test
    REQUIRE(ToString(aare::DetectorType::Jungfrau) == "Jungfrau");
}

TEST_CASE("Cluster creation") {
    aare::Cluster c(13, 15);
    REQUIRE(c.cluster_sizeX == 13);
    REQUIRE(c.cluster_sizeY == 15);
    REQUIRE(c.dt == aare::Dtype(typeid(int32_t)));
    REQUIRE(c.data() != nullptr);

    aare::Cluster c2(c);
    REQUIRE(c2.cluster_sizeX == 13);
    REQUIRE(c2.cluster_sizeY == 15);
    REQUIRE(c2.dt == aare::Dtype(typeid(int32_t)));
    REQUIRE(c2.data() != nullptr);
}

// TEST_CASE("cluster set and get data") {

//     aare::Cluster c2(33, 44, aare::Dtype(typeid(double)));
//     REQUIRE(c2.cluster_sizeX == 33);
//     REQUIRE(c2.cluster_sizeY == 44);
//     REQUIRE(c2.dt == aare::Dtype::DOUBLE);
//     double v = 3.14;
//     c2.set<double>(0, v);
//     double v2 = c2.get<double>(0);
//     REQUIRE(aare::compare_floats<double>(v, v2));

//     c2.set<double>(33 * 44 - 1, 123.11);
//     double v3 = c2.get<double>(33 * 44 - 1);
//     REQUIRE(aare::compare_floats<double>(123.11, v3));

//     REQUIRE_THROWS_AS(c2.set(0, 1), std::invalid_argument); // set int to double
// }