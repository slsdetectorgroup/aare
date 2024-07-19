#include "aare/core/defs.hpp"
#include "aare/core/Cluster.hpp"
#include "aare/utils/floats.hpp"
#include <catch2/catch_test_macros.hpp>
#include <string>
TEST_CASE("Enum to string conversion") {
    // By the way I don't think the enum string conversions should be in the defs.hpp file
    // but let's use this to show a test
    REQUIRE(toString(aare::DetectorType::Jungfrau) == "Jungfrau");
}
