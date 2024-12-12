#include <cstdint>
#include "aare/ClusterVector.hpp"

#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <catch2/catch_test_macros.hpp>

using aare::ClusterVector;

TEST_CASE("ClusterVector 2x2 int32_t capacity 4, push back then read") {
    struct Cluster_i2x2 {
        int16_t x;
        int16_t y;
        int32_t data[4];
    };

    ClusterVector<int32_t> cv(2, 2, 4);
    REQUIRE(cv.capacity() == 4);
    REQUIRE(cv.size() == 0);
    REQUIRE(cv.cluster_size_x() == 2);
    REQUIRE(cv.cluster_size_y() == 2);
    // int16_t, int16_t, 2x2 int32_t = 20 bytes
    REQUIRE(cv.element_offset() == 20);

    //Create a cluster and push back into the vector
    Cluster_i2x2 c1 = {1, 2, {3, 4, 5, 6}};
    cv.push_back(c1.x, c1.y, reinterpret_cast<std::byte*>(&c1.data[0]));
    REQUIRE(cv.size() == 1);
    REQUIRE(cv.capacity() == 4);

    //Read the cluster back out using copy. TODO! Can we improve the API?
    Cluster_i2x2 c2;
    std::byte *ptr = cv.element_ptr(0);
    std::copy(ptr, ptr + cv.element_offset(), reinterpret_cast<std::byte*>(&c2));

    //Check that the data is the same
    REQUIRE(c1.x == c2.x);
    REQUIRE(c1.y == c2.y);
    for(size_t i = 0; i < 4; i++) {
        REQUIRE(c1.data[i] == c2.data[i]);
    }
}

TEST_CASE("Summing 3x1 clusters of int64"){
    struct Cluster_l3x1{
        int16_t x;
        int16_t y;
        int32_t data[3];
    };

    ClusterVector<int32_t> cv(3, 1, 2);
    REQUIRE(cv.capacity() == 2);
    REQUIRE(cv.size() == 0);
    REQUIRE(cv.cluster_size_x() == 3);
    REQUIRE(cv.cluster_size_y() == 1);

    //Create a cluster and push back into the vector
    Cluster_l3x1 c1 = {1, 2, {3, 4, 5}};
    cv.push_back(c1.x, c1.y, reinterpret_cast<std::byte*>(&c1.data[0]));
    REQUIRE(cv.capacity() == 2);
    REQUIRE(cv.size() == 1);

    Cluster_l3x1 c2 = {6, 7, {8, 9, 10}};
    cv.push_back(c2.x, c2.y, reinterpret_cast<std::byte*>(&c2.data[0]));
    REQUIRE(cv.capacity() == 2);
    REQUIRE(cv.size() == 2);

    Cluster_l3x1 c3 = {11, 12, {13, 14, 15}};
    cv.push_back(c3.x, c3.y, reinterpret_cast<std::byte*>(&c3.data[0]));
    REQUIRE(cv.capacity() == 4);
    REQUIRE(cv.size() == 3);

    auto sums = cv.sum();
    REQUIRE(sums.size() == 3);
    REQUIRE(sums[0] == 12);
    REQUIRE(sums[1] == 27);
    REQUIRE(sums[2] == 42);
}

TEST_CASE("Storing floats"){
    struct Cluster_f4x2{
        int16_t x;
        int16_t y;
        float data[8];
    };

    ClusterVector<float> cv(2, 4, 2);
    REQUIRE(cv.capacity() == 2);
    REQUIRE(cv.size() == 0);
    REQUIRE(cv.cluster_size_x() == 2);
    REQUIRE(cv.cluster_size_y() == 4);

    //Create a cluster and push back into the vector
    Cluster_f4x2 c1 = {1, 2, {3.0, 4.0, 5.0, 6.0,3.0, 4.0, 5.0, 6.0}};
    cv.push_back(c1.x, c1.y, reinterpret_cast<std::byte*>(&c1.data[0]));
    REQUIRE(cv.capacity() == 2);
    REQUIRE(cv.size() == 1);
    

    Cluster_f4x2 c2 = {6, 7, {8.0, 9.0, 10.0, 11.0,8.0, 9.0, 10.0, 11.0}};
    cv.push_back(c2.x, c2.y, reinterpret_cast<std::byte*>(&c2.data[0]));
    REQUIRE(cv.capacity() == 2);
    REQUIRE(cv.size() == 2);

    auto sums = cv.sum();
    REQUIRE(sums.size() == 2);
    REQUIRE_THAT(sums[0], Catch::Matchers::WithinAbs(36.0, 1e-6));
    REQUIRE_THAT(sums[1], Catch::Matchers::WithinAbs(76.0, 1e-6));
}