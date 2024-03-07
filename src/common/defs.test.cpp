#include <catch2/catch_test_macros.hpp>
#include <string>
#include "defs.hpp"
TEST_CASE("Enum to string conversion"){
    //By the way I don't think the enum string conversions should be in the defs.hpp file
    //but let's use this to show a test
    REQUIRE(toString(DetectorType::Jungfrau) == "Jungfrau");
}