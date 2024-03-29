#include <catch2/catch_test_macros.hpp>
#include "aare/NumpyHelpers.hpp" //Is this really a public header?

using namespace aare::NumpyHelpers; 

TEST_CASE("is_digits with a few standard cases"){
    REQUIRE(is_digits(""));
    REQUIRE(is_digits("123"));
    REQUIRE(is_digits("0"));
    REQUIRE_FALSE(is_digits("hej123"));
    REQUIRE_FALSE(is_digits("a"));
    REQUIRE_FALSE(is_digits(" "));
    REQUIRE_FALSE(is_digits("abcdef"));
}

TEST_CASE("Check for quotes and return stripped string"){
    REQUIRE(parse_str("'hej'") == "hej");    
    REQUIRE(parse_str("'hej hej'") == "hej hej");    
    REQUIRE(parse_str("''") == "");    
}

TEST_CASE("parsing a string without quotes throws"){
    REQUIRE_THROWS(parse_str("hej"));
}

TEST_CASE("trim whitespace"){
    REQUIRE(trim(" hej ") == "hej");
    REQUIRE(trim("hej") == "hej");
    REQUIRE(trim(" hej") == "hej");
    REQUIRE(trim("hej ") == "hej");
    REQUIRE(trim(" ") == "");
    REQUIRE(trim(" \thej hej ") == "hej hej");
}