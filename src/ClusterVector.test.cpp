#include "aare/ClusterVector.hpp"
#include <cstdint>

#include <catch2/catch_all.hpp>
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

using aare::Cluster;
using aare::ClusterVector;
using C1 = Cluster<int32_t, 2, 2>;


TEST_CASE("A newly created ClusterVector is empty") {
    ClusterVector<C1> cv(4);
    REQUIRE(cv.empty());
}

TEST_CASE("After pushing back one element the ClusterVector is not empty") {
    ClusterVector<C1> cv(4);
    cv.push_back(C1{1, 2, {3, 4}});
    REQUIRE(!cv.empty());
}

TEST_CASE("item_size return the size of the cluster stored") {
    ClusterVector<C1> cv(4);
    CHECK(cv.item_size() == sizeof(C1));

    // Sanity check
    // 2*2*4 = 16 bytes of data for the cluster
    //  2*2 = 4 bytes for the x and y coordinates
    REQUIRE(cv.item_size() == 20);

    using C2 = Cluster<int32_t, 3, 3>;
    ClusterVector<C2> cv2(4);
    CHECK(cv2.item_size() == sizeof(C2));

    using C3 = Cluster<double, 2, 3>;
    ClusterVector<C3> cv3(4);
    CHECK(cv3.item_size() == sizeof(C3));

    using C4 = Cluster<char, 10, 5>;
    ClusterVector<C4> cv4(4);
    CHECK(cv4.item_size() == sizeof(C4));

    using C5 = Cluster<int32_t, 2, 3>;
    ClusterVector<C5> cv5(4);
    CHECK(cv5.item_size() == sizeof(C5));

    using C6 = Cluster<double, 5, 5>;
    ClusterVector<C6> cv6(4);
    CHECK(cv6.item_size() == sizeof(C6));

    using C7 = Cluster<double, 3, 3>;
    ClusterVector<C7> cv7(4);
    CHECK(cv7.item_size() == sizeof(C7));
}

TEST_CASE("ClusterVector 2x2 int32_t capacity 4, push back then read") {

    ClusterVector<Cluster<int32_t, 2, 2>> cv(4);
    REQUIRE(cv.capacity() == 4);
    REQUIRE(cv.size() == 0);
    REQUIRE(cv.cluster_size_x() == 2);
    REQUIRE(cv.cluster_size_y() == 2);
    // int16_t, int16_t, 2x2 int32_t = 20 bytes
    REQUIRE(cv.item_size() == 20);

    // Create a cluster and push back into the vector
    Cluster<int32_t, 2, 2> c1 = {1, 2, {3, 4, 5, 6}};
    cv.push_back(c1);
    REQUIRE(cv.size() == 1);
    REQUIRE(cv.capacity() == 4);

    auto c2 = cv[0];

    // Check that the data is the same
    REQUIRE(c1.x == c2.x);
    REQUIRE(c1.y == c2.y);
    for (size_t i = 0; i < 4; i++) {
        REQUIRE(c1.data[i] == c2.data[i]);
    }
}

TEST_CASE("Summing 3x1 clusters of int64") {
    ClusterVector<Cluster<int32_t, 3, 1>> cv(2);
    REQUIRE(cv.capacity() == 2);
    REQUIRE(cv.size() == 0);
    REQUIRE(cv.cluster_size_x() == 3);
    REQUIRE(cv.cluster_size_y() == 1);

    // Create a cluster and push back into the vector
    Cluster<int32_t, 3, 1> c1 = {1, 2, {3, 4, 5}};
    cv.push_back(c1);
    REQUIRE(cv.capacity() == 2);
    REQUIRE(cv.size() == 1);

    Cluster<int32_t, 3, 1> c2 = {6, 7, {8, 9, 10}};
    cv.push_back(c2);
    REQUIRE(cv.capacity() == 2);
    REQUIRE(cv.size() == 2);

    Cluster<int32_t, 3, 1> c3 = {11, 12, {13, 14, 15}};
    cv.push_back(c3);
    REQUIRE(cv.capacity() == 4);
    REQUIRE(cv.size() == 3);

    /*
    auto sums = cv.sum();
    REQUIRE(sums.size() == 3);
    REQUIRE(sums[0] == 12);
    REQUIRE(sums[1] == 27);
    REQUIRE(sums[2] == 42);
    */
}

TEST_CASE("Storing floats") {
    ClusterVector<Cluster<float, 2, 4>> cv(10);
    REQUIRE(cv.capacity() == 10);
    REQUIRE(cv.size() == 0);
    REQUIRE(cv.cluster_size_x() == 2);
    REQUIRE(cv.cluster_size_y() == 4);

    // Create a cluster and push back into the vector
    Cluster<float, 2, 4> c1 = {1, 2, {3.0, 4.0, 5.0, 6.0, 3.0, 4.0, 5.0, 6.0}};
    cv.push_back(c1);
    REQUIRE(cv.capacity() == 10);
    REQUIRE(cv.size() == 1);

    Cluster<float, 2, 4> c2 = {
        6, 7, {8.0, 9.0, 10.0, 11.0, 8.0, 9.0, 10.0, 11.0}};
    cv.push_back(c2);
    REQUIRE(cv.capacity() == 10);
    REQUIRE(cv.size() == 2);

    /*
    auto sums = cv.sum();
    REQUIRE(sums.size() == 2);
    REQUIRE_THAT(sums[0], Catch::Matchers::WithinAbs(36.0, 1e-6));
    REQUIRE_THAT(sums[1], Catch::Matchers::WithinAbs(76.0, 1e-6));
    */
}

TEST_CASE("Push back more than initial capacity") {

    ClusterVector<Cluster<int32_t, 2, 2>> cv(2);
    auto initial_data = cv.data();
    Cluster<int32_t, 2, 2> c1 = {1, 2, {3, 4, 5, 6}};
    cv.push_back(c1);
    REQUIRE(cv.size() == 1);
    REQUIRE(cv.capacity() == 2);

    Cluster<int32_t, 2, 2> c2 = {6, 7, {8, 9, 10, 11}};
    cv.push_back(c2);
    REQUIRE(cv.size() == 2);
    REQUIRE(cv.capacity() == 2);

    Cluster<int32_t, 2, 2> c3 = {11, 12, {13, 14, 15, 16}};
    cv.push_back(c3);
    REQUIRE(cv.size() == 3);
    REQUIRE(cv.capacity() == 4);

    Cluster<int32_t, 2, 2> *ptr =
        reinterpret_cast<Cluster<int32_t, 2, 2> *>(cv.data());
    REQUIRE(ptr[0].x == 1);
    REQUIRE(ptr[0].y == 2);
    REQUIRE(ptr[1].x == 6);
    REQUIRE(ptr[1].y == 7);
    REQUIRE(ptr[2].x == 11);
    REQUIRE(ptr[2].y == 12);

    // We should have allocated a new buffer, since we outgrew the initial
    // capacity
    REQUIRE(initial_data != cv.data());
}

TEST_CASE("Concatenate two cluster vectors where the first has enough capacity") {
    ClusterVector<Cluster<int32_t, 2, 2>> cv1(12);
    Cluster<int32_t, 2, 2> c1 = {1, 2, {3, 4, 5, 6}};
    cv1.push_back(c1);
    Cluster<int32_t, 2, 2> c2 = {6, 7, {8, 9, 10, 11}};
    cv1.push_back(c2);

    ClusterVector<Cluster<int32_t, 2, 2>> cv2(2);
    Cluster<int32_t, 2, 2> c3 = {11, 12, {13, 14, 15, 16}};
    cv2.push_back(c3);
    Cluster<int32_t, 2, 2> c4 = {16, 17, {18, 19, 20, 21}};
    cv2.push_back(c4);

    cv1 += cv2;
    REQUIRE(cv1.size() == 4);
    REQUIRE(cv1.capacity() == 12);

    Cluster<int32_t, 2, 2> *ptr =
        reinterpret_cast<Cluster<int32_t, 2, 2> *>(cv1.data());
    REQUIRE(ptr[0].x == 1);
    REQUIRE(ptr[0].y == 2);
    REQUIRE(ptr[1].x == 6);
    REQUIRE(ptr[1].y == 7);
    REQUIRE(ptr[2].x == 11);
    REQUIRE(ptr[2].y == 12);
    REQUIRE(ptr[3].x == 16);
    REQUIRE(ptr[3].y == 17);
}

TEST_CASE("Concatenate two cluster vectors where we need to allocate") {
    ClusterVector<Cluster<int32_t, 2, 2>> cv1(2);
    Cluster<int32_t, 2, 2> c1 = {1, 2, {3, 4, 5, 6}};
    cv1.push_back(c1);
    Cluster<int32_t, 2, 2> c2 = {6, 7, {8, 9, 10, 11}};
    cv1.push_back(c2);

    ClusterVector<Cluster<int32_t, 2, 2>> cv2(2);
    Cluster<int32_t, 2, 2> c3 = {11, 12, {13, 14, 15, 16}};
    cv2.push_back(c3);
    Cluster<int32_t, 2, 2> c4 = {16, 17, {18, 19, 20, 21}};
    cv2.push_back(c4);

    cv1 += cv2;
    REQUIRE(cv1.size() == 4);
    REQUIRE(cv1.capacity() == 4);

    Cluster<int32_t, 2, 2> *ptr =
        reinterpret_cast<Cluster<int32_t, 2, 2> *>(cv1.data());
    REQUIRE(ptr[0].x == 1);
    REQUIRE(ptr[0].y == 2);
    REQUIRE(ptr[1].x == 6);
    REQUIRE(ptr[1].y == 7);
    REQUIRE(ptr[2].x == 11);
    REQUIRE(ptr[2].y == 12);
    REQUIRE(ptr[3].x == 16);
    REQUIRE(ptr[3].y == 17);
}

struct ClusterTestData {
    uint8_t ClusterSizeX;
    uint8_t ClusterSizeY;
    std::vector<int64_t> index_map_x;
    std::vector<int64_t> index_map_y;
};

TEST_CASE("Gain Map Calculation Index Map") {

    auto clustertestdata = GENERATE(
        ClusterTestData{3,
                        3,
                        {-1, 0, 1, -1, 0, 1, -1, 0, 1},
                        {-1, -1, -1, 0, 0, 0, 1, 1, 1}},
        ClusterTestData{
            4,
            4,
            {-2, -1, 0, 1, -2, -1, 0, 1, -2, -1, 0, 1, -2, -1, 0, 1},
            {-2, -2, -2, -2, -1, -1, -1, -1, 0, 0, 0, 0, 1, 1, 1, 1}},
        ClusterTestData{2, 2, {-1, 0, -1, 0}, {-1, -1, 0, 0}},
        ClusterTestData{5,
                        5,
                        {-2, -1, 0,  1,  2, -2, -1, 0,  1,  2, -2, -1, 0,
                         1,  2,  -2, -1, 0, 1,  2,  -2, -1, 0, 1,  2},
                        {-2, -2, -2, -2, -2, -1, -1, -1, -1, -1, 0, 0, 0,
                         0,  0,  1,  1,  1,  1,  1,  2,  2,  2,  2, 2}});

    uint8_t ClusterSizeX = clustertestdata.ClusterSizeX;
    uint8_t ClusterSizeY = clustertestdata.ClusterSizeY;

    std::vector<int64_t> index_map_x(ClusterSizeX * ClusterSizeY);
    std::vector<int64_t> index_map_y(ClusterSizeX * ClusterSizeY);

    int64_t index_cluster_center_x = ClusterSizeX / 2;
    int64_t index_cluster_center_y = ClusterSizeY / 2;

    for (size_t j = 0; j < ClusterSizeX * ClusterSizeY; j++) {
        index_map_x[j] = j % ClusterSizeX - index_cluster_center_x;
        index_map_y[j] = j / ClusterSizeX - index_cluster_center_y;
    }

    CHECK(index_map_x == clustertestdata.index_map_x);
    CHECK(index_map_y == clustertestdata.index_map_y);
}