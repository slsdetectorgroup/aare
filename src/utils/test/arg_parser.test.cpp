#include "aare/utils/arg_parser.hpp" // Include the arg_parser.hpp header file
#include <catch2/catch_test_macros.hpp>
#include <iostream>
#include <string>
#include <vector>

using namespace aare;

TEST_CASE("") {
    ArgParser parser("Test parser");

    SECTION("Test Default values") {
        parser.add_option("option1", "o1",true,  false, "default1", "Description of option1");
        parser.add_option("option2", "o2",true,  false, "default2", "Description of option2");
        parser.add_option("option3", "o3",true,  false, "default3", "Description of option3");
        parser.add_option("option4", "o4",false, false,  "0", "Description of option4");
        parser.add_option("option5", "o5",false, false,  "", "Description of option5");

        char *argv[] = {(char *)"filename"};
        auto args = parser.parse(1, argv);
        REQUIRE(args["option1"] == "default1");
        REQUIRE(args["option2"] == "default2");
        REQUIRE(args["option3"] == "default3");
        REQUIRE(args["option4"] == "0");
        REQUIRE(args["option5"] == "0");

        
    }
    SECTION("Test normal usage") {
        parser.add_option("option1", "o1", true, false, "default1", "Description of option1");
        parser.add_option("option2", "o2", false, false, "", "Description of option2");
        parser.add_option("option3", "o3", true, true, "", "Description of option3");
        parser.add_option("option4", "o4", false, false, "", "Description of option4");
        char *argv[] = {(char *)"filename", (char *)"--option3", (char *)"value3", (char *)"-o4"};
        auto args = parser.parse(4, argv);
        REQUIRE(args["option1"] == "default1");
        REQUIRE(args["option2"] == "0");
        REQUIRE(args["option3"] == "value3");
        REQUIRE(args["option4"] == "1");
    }

}