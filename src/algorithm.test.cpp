

#include <catch2/catch_test_macros.hpp>
#include <aare/algorithm.hpp>


TEST_CASE("Find the closed index in a 1D array", "[algorithm]") {
    aare::NDArray<double, 1> arr({5});
    for (size_t i = 0; i < arr.size(); i++) {
        arr[i] = i;
    }
    // arr 0, 1, 2, 3, 4
    REQUIRE(aare::nearest_index(arr, 2.3) == 2);
    REQUIRE(aare::nearest_index(arr, 2.6) == 3);
    REQUIRE(aare::nearest_index(arr, 45.0) == 4);
    REQUIRE(aare::nearest_index(arr, 0.0) == 0);
    REQUIRE(aare::nearest_index(arr, -1.0) == 0);
}

TEST_CASE("Passing integers to nearest_index works"){
    aare::NDArray<int, 1> arr({5});
    for (size_t i = 0; i < arr.size(); i++) {
        arr[i] = i;
    }
    // arr 0, 1, 2, 3, 4
    REQUIRE(aare::nearest_index(arr, 2) == 2);
    REQUIRE(aare::nearest_index(arr, 3) == 3);
    REQUIRE(aare::nearest_index(arr, 45) == 4);
    REQUIRE(aare::nearest_index(arr, 0) == 0);
    REQUIRE(aare::nearest_index(arr, -1) == 0);
}


TEST_CASE("nearest_index works with std::vector"){
    std::vector<double> vec = {0, 1, 2, 3, 4};
    REQUIRE(aare::nearest_index(vec, 2.123) == 2);
    REQUIRE(aare::nearest_index(vec, 2.66) == 3);
    REQUIRE(aare::nearest_index(vec, 4555555.0) == 4);
    REQUIRE(aare::nearest_index(vec, 0.0) == 0);
    REQUIRE(aare::nearest_index(vec, -10.0) == 0);
}

TEST_CASE("nearest index works with std::array"){
    std::array<double, 5> arr = {0, 1, 2, 3, 4};
    REQUIRE(aare::nearest_index(arr, 2.123) == 2);
    REQUIRE(aare::nearest_index(arr, 2.501) == 3);
    REQUIRE(aare::nearest_index(arr, 4555555.0) == 4);
    REQUIRE(aare::nearest_index(arr, 0.0) == 0);
    REQUIRE(aare::nearest_index(arr, -10.0) == 0);
}


TEST_CASE("last smaller"){
    aare::NDArray<double, 1> arr({5});
    for (size_t i = 0; i < arr.size(); i++) {
        arr[i] = i;
    }
    // arr 0, 1, 2, 3, 4
    REQUIRE(aare::last_smaller(arr, -10.0) == 0);
    REQUIRE(aare::last_smaller(arr, 0.0) == 0);
    REQUIRE(aare::last_smaller(arr, 2.3) == 2);
    REQUIRE(aare::last_smaller(arr, 253.) == 4);
}