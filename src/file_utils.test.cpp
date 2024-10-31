#include "aare/file_utils.hpp"

#include <catch2/catch_test_macros.hpp>

using namespace aare;

TEST_CASE("Use filename to determine if it is a master file") {

    REQUIRE(is_master_file("test_master_1.json"));
}

TEST_CASE("Parse a master file fname"){
    auto fnc = parse_fname("test_master_1.json");
    REQUIRE(fnc.base_name == "test");
    REQUIRE(fnc.ext == ".json");
    REQUIRE(fnc.findex == 1);

    REQUIRE(fnc.master_fname() == "test_master_1.json");
    REQUIRE(fnc.data_fname(1, 2) == "test_d2_f1_1.raw");
}