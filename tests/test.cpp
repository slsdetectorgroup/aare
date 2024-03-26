#include <catch2/catch_test_macros.hpp>
#include <filesystem>
#include <fstream>

#include "test_config.hpp"

TEST_CASE("Test suite can find data assets") {
    auto fpath = test_data_path() / "numpy" / "test_numpy_file.npy";
    REQUIRE(std::filesystem::exists(fpath));
}

TEST_CASE("Test suite can open data assets") {
    auto fpath = test_data_path() / "numpy" / "test_numpy_file.npy";
    auto f = std::ifstream(fpath, std::ios::binary);
    REQUIRE(f.is_open());
}