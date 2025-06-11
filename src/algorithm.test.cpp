

#include <aare/algorithm.hpp>
#include <catch2/catch_test_macros.hpp>

TEST_CASE("Find the closed index in a 1D array", "[algorithm]") {
    aare::NDArray<double, 1> arr({5});
    for (ssize_t i = 0; i < arr.size(); i++) {
        arr[i] = i;
    }
    // arr 0, 1, 2, 3, 4
    REQUIRE(aare::nearest_index(arr, 2.3) == 2);
    REQUIRE(aare::nearest_index(arr, 2.6) == 3);
    REQUIRE(aare::nearest_index(arr, 45.0) == 4);
    REQUIRE(aare::nearest_index(arr, 0.0) == 0);
    REQUIRE(aare::nearest_index(arr, -1.0) == 0);
}

TEST_CASE("Passing integers to nearest_index works", "[algorithm]") {
    aare::NDArray<int, 1> arr({5});
    for (ssize_t i = 0; i < arr.size(); i++) {
        arr[i] = i;
    }
    // arr 0, 1, 2, 3, 4
    REQUIRE(aare::nearest_index(arr, 2) == 2);
    REQUIRE(aare::nearest_index(arr, 3) == 3);
    REQUIRE(aare::nearest_index(arr, 45) == 4);
    REQUIRE(aare::nearest_index(arr, 0) == 0);
    REQUIRE(aare::nearest_index(arr, -1) == 0);
}

TEST_CASE("nearest_index works with std::vector", "[algorithm]") {
    std::vector<double> vec = {0, 1, 2, 3, 4};
    REQUIRE(aare::nearest_index(vec, 2.123) == 2);
    REQUIRE(aare::nearest_index(vec, 2.66) == 3);
    REQUIRE(aare::nearest_index(vec, 4555555.0) == 4);
    REQUIRE(aare::nearest_index(vec, 0.0) == 0);
    REQUIRE(aare::nearest_index(vec, -10.0) == 0);
}

TEST_CASE("nearest index works with std::array", "[algorithm]") {
    std::array<double, 5> arr = {0, 1, 2, 3, 4};
    REQUIRE(aare::nearest_index(arr, 2.123) == 2);
    REQUIRE(aare::nearest_index(arr, 2.501) == 3);
    REQUIRE(aare::nearest_index(arr, 4555555.0) == 4);
    REQUIRE(aare::nearest_index(arr, 0.0) == 0);
    REQUIRE(aare::nearest_index(arr, -10.0) == 0);
}

TEST_CASE("nearest index when there is no different uses the first element",
          "[algorithm]") {
    std::vector<int> vec = {5, 5, 5, 5, 5};
    REQUIRE(aare::nearest_index(vec, 5) == 0);
}

TEST_CASE("nearest index when there is no different uses the first element "
          "also when all smaller",
          "[algorithm]") {
    std::vector<int> vec = {5, 5, 5, 5, 5};
    REQUIRE(aare::nearest_index(vec, 10) == 0);
}

TEST_CASE("last smaller", "[algorithm]") {
    aare::NDArray<double, 1> arr({5});
    for (ssize_t i = 0; i < arr.size(); i++) {
        arr[i] = i;
    }
    // arr 0, 1, 2, 3, 4
    REQUIRE(aare::last_smaller(arr, -10.0) == 0);
    REQUIRE(aare::last_smaller(arr, 0.0) == 0);
    REQUIRE(aare::last_smaller(arr, 2.3) == 2);
    REQUIRE(aare::last_smaller(arr, 253.) == 4);
}

TEST_CASE("returns last bin strictly smaller", "[algorithm]") {
    aare::NDArray<double, 1> arr({5});
    for (ssize_t i = 0; i < arr.size(); i++) {
        arr[i] = i;
    }
    // arr 0, 1, 2, 3, 4
    REQUIRE(aare::last_smaller(arr, 2.0) == 1);
}

TEST_CASE("last_smaller with all elements smaller returns last element",
          "[algorithm]") {
    aare::NDArray<double, 1> arr({5});
    for (ssize_t i = 0; i < arr.size(); i++) {
        arr[i] = i;
    }
    // arr 0, 1, 2, 3, 4
    REQUIRE(aare::last_smaller(arr, 50.) == 4);
}

TEST_CASE("last_smaller with all elements bigger returns first element",
          "[algorithm]") {
    aare::NDArray<double, 1> arr({5});
    for (ssize_t i = 0; i < arr.size(); i++) {
        arr[i] = i;
    }
    // arr 0, 1, 2, 3, 4
    REQUIRE(aare::last_smaller(arr, -50.) == 0);
}

TEST_CASE("last smaller with all elements equal returns the first element",
          "[algorithm]") {
    std::vector<int> vec = {5, 5, 5, 5, 5, 5, 5};
    REQUIRE(aare::last_smaller(vec, 5) == 0);
}

TEST_CASE("first_lager with vector", "[algorithm]") {
    std::vector<double> vec = {0, 1, 2, 3, 4};
    REQUIRE(aare::first_larger(vec, 2.5) == 3);
}

TEST_CASE("first_lager with all elements smaller returns last element",
          "[algorithm]") {
    std::vector<double> vec = {0, 1, 2, 3, 4};
    REQUIRE(aare::first_larger(vec, 50.) == 4);
}

TEST_CASE("first_lager with all elements bigger returns first element",
          "[algorithm]") {
    std::vector<double> vec = {0, 1, 2, 3, 4};
    REQUIRE(aare::first_larger(vec, -50.) == 0);
}

TEST_CASE("first_lager with all elements the same as the check returns last",
          "[algorithm]") {
    std::vector<int> vec = {14, 14, 14, 14, 14};
    REQUIRE(aare::first_larger(vec, 14) == 4);
}

TEST_CASE("first larger with the same element", "[algorithm]") {
    std::vector<int> vec = {7, 8, 9, 10, 11};
    REQUIRE(aare::first_larger(vec, 9) == 3);
}

TEST_CASE("cumsum works", "[algorithm]") {
    std::vector<double> vec = {0, 1, 2, 3, 4};
    auto result = aare::cumsum(vec);
    REQUIRE(result.size() == vec.size());
    REQUIRE(result[0] == 0);
    REQUIRE(result[1] == 1);
    REQUIRE(result[2] == 3);
    REQUIRE(result[3] == 6);
    REQUIRE(result[4] == 10);
}
TEST_CASE("cumsum works with empty vector", "[algorithm]") {
    std::vector<double> vec = {};
    auto result = aare::cumsum(vec);
    REQUIRE(result.size() == 0);
}
TEST_CASE("cumsum works with negative numbers", "[algorithm]") {
    std::vector<double> vec = {0, -1, -2, -3, -4};
    auto result = aare::cumsum(vec);
    REQUIRE(result.size() == vec.size());
    REQUIRE(result[0] == 0);
    REQUIRE(result[1] == -1);
    REQUIRE(result[2] == -3);
    REQUIRE(result[3] == -6);
    REQUIRE(result[4] == -10);
}

TEST_CASE("cumsum on an empty vector", "[algorithm]") {
    std::vector<double> vec = {};
    auto result = aare::cumsum(vec);
    REQUIRE(result.size() == 0);
}

TEST_CASE("All equal on an empty vector is false", "[algorithm]") {
    std::vector<int> vec = {};
    REQUIRE(aare::all_equal(vec) == false);
}

TEST_CASE("All equal on a vector with 1 element is true", "[algorithm]") {
    std::vector<int> vec = {1};
    REQUIRE(aare::all_equal(vec) == true);
}

TEST_CASE("All equal on a vector with 2 elements is true", "[algorithm]") {
    std::vector<int> vec = {1, 1};
    REQUIRE(aare::all_equal(vec) == true);
}

TEST_CASE("All equal on a vector with two different  elements is false",
          "[algorithm]") {
    std::vector<int> vec = {1, 2};
    REQUIRE(aare::all_equal(vec) == false);
}

TEST_CASE("Last element is different", "[algorithm]") {
    std::vector<int> vec = {1, 1, 1, 1, 2};
    REQUIRE(aare::all_equal(vec) == false);
}
