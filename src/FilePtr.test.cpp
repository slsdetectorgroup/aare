// SPDX-License-Identifier: MPL-2.0

#include "aare/FilePtr.hpp"

#include <catch2/catch_test_macros.hpp>
#include <utility>

TEST_CASE("FilePtr converts to its open state", "[FilePtr]") {
    aare::FilePtr empty;
    CHECK_FALSE(empty);

    aare::FilePtr open(__FILE__, "rb");
    CHECK(open);

    aare::FilePtr moved(std::move(open));
    CHECK(moved);
    CHECK_FALSE(open);
}
