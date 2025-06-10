#include "aare/NumpyHelpers.hpp" //Is this really a public header?
#include <catch2/catch_test_macros.hpp>

using namespace aare::NumpyHelpers;

TEST_CASE("is_digits with a few standard cases") {
    REQUIRE(is_digits(""));
    REQUIRE(is_digits("123"));
    REQUIRE(is_digits("0"));
    REQUIRE_FALSE(is_digits("hej123"));
    REQUIRE_FALSE(is_digits("a"));
    REQUIRE_FALSE(is_digits(" "));
    REQUIRE_FALSE(is_digits("abcdef"));
}

TEST_CASE("Check for quotes and return stripped string") {
    REQUIRE(parse_str("'hej'") == "hej");
    REQUIRE(parse_str("'hej hej'") == "hej hej");
    REQUIRE(parse_str("''") == "");
}

TEST_CASE("parsing a string without quotes throws") {
    REQUIRE_THROWS(parse_str("hej"));
}

TEST_CASE("trim whitespace") {
    REQUIRE(trim(" hej ") == "hej");
    REQUIRE(trim("hej") == "hej");
    REQUIRE(trim(" hej") == "hej");
    REQUIRE(trim("hej ") == "hej");
    REQUIRE(trim(" ") == "");
    REQUIRE(trim(" \thej hej ") == "hej hej");
}

TEST_CASE("parse data type descriptions") {
    REQUIRE(parse_descr("<i1") == aare::Dtype::INT8);
    REQUIRE(parse_descr("<i2") == aare::Dtype::INT16);
    REQUIRE(parse_descr("<i4") == aare::Dtype::INT32);
    REQUIRE(parse_descr("<i8") == aare::Dtype::INT64);

    REQUIRE(parse_descr("<u1") == aare::Dtype::UINT8);
    REQUIRE(parse_descr("<u2") == aare::Dtype::UINT16);
    REQUIRE(parse_descr("<u4") == aare::Dtype::UINT32);
    REQUIRE(parse_descr("<u8") == aare::Dtype::UINT64);

    REQUIRE(parse_descr("<f4") == aare::Dtype::FLOAT);
    REQUIRE(parse_descr("<f8") == aare::Dtype::DOUBLE);
}

TEST_CASE("is element in array") {
    REQUIRE(in_array(1, std::array<int, 3>{1, 2, 3}));
    REQUIRE_FALSE(in_array(4, std::array<int, 3>{1, 2, 3}));
    REQUIRE(in_array(1, std::array<int, 1>{1}));
    REQUIRE_FALSE(in_array(1, std::array<int, 0>{}));
}

TEST_CASE("Parse numpy dict") {
    std::string in =
        "{'descr': '<f4', 'fortran_order': False, 'shape': (3, 4)}";
    std::vector<std::string> keys{"descr", "fortran_order", "shape"};
    auto map = parse_dict(in, keys);
    REQUIRE(map["descr"] == "'<f4'");
    REQUIRE(map["fortran_order"] == "False");
    REQUIRE(map["shape"] == "(3, 4)");
}