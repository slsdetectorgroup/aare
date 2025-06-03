#include "test_config.hpp"
#include <catch2/catch_test_macros.hpp>
#include <climits>
#include <filesystem>
#include <fstream>
#include <fmt/core.h>

TEST_CASE("Test suite can find data assets", "[.integration]") {
    auto fpath = test_data_path() / "numpy" / "test_numpy_file.npy";
    REQUIRE(std::filesystem::exists(fpath));
}

TEST_CASE("Test suite can open data assets", "[.integration]") {
    auto fpath = test_data_path() / "numpy" / "test_numpy_file.npy";
    auto f = std::ifstream(fpath, std::ios::binary);
    REQUIRE(f.is_open());
}

TEST_CASE("Test float32 and char8") {
    REQUIRE(sizeof(float) == 4);
    REQUIRE(CHAR_BIT == 8);
}

/**
 * Uncomment the following tests to verify that asan is working
 */

// TEST_CASE("trigger asan stack"){
//     int arr[5] =  {1,2,3,4,5};
//     int val = arr[7];
//     fmt::print("val: {}\n", val);
// }

// TEST_CASE("trigger asan heap"){
//     auto *ptr = new int[5];
//     ptr[70] = 5;
//     fmt::print("ptr: {}\n", ptr[70]);
// }