#include "test_config.hpp"
#include <catch2/catch_test_macros.hpp>
#include <climits>
#include <filesystem>
#include <fstream>

TEST_CASE("Test suite can find data assets") {
    auto fpath = test_data_path() / "numpy" / "test_numpy_file.npy";
    REQUIRE(std::filesystem::exists(fpath));
}

TEST_CASE("Test suite can open data assets") {
    auto fpath = test_data_path() / "numpy" / "test_numpy_file.npy";
    auto f = std::ifstream(fpath, std::ios::binary);
    REQUIRE(f.is_open());
}

TEST_CASE("Test float32 and char8") {
    REQUIRE(sizeof(float) == 4);
    REQUIRE(CHAR_BIT == 8);
}