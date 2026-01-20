

#include "aare/Dtype.hpp"
#include <catch2/catch_test_macros.hpp>

using aare::Dtype;
using aare::endian;

TEST_CASE("Construct from typeid") {
    REQUIRE(Dtype(typeid(int)) == typeid(int));
    REQUIRE(Dtype(typeid(int)) != typeid(double));
}

TEST_CASE("Construct from string") {
    if (endian::native == endian::little) {
        REQUIRE(Dtype("<i1") == typeid(int8_t));
        REQUIRE(Dtype("<u1") == typeid(uint8_t));
        REQUIRE(Dtype("<i2") == typeid(int16_t));
        REQUIRE(Dtype("<u2") == typeid(uint16_t));
        REQUIRE(Dtype("<i4") == typeid(int));
        REQUIRE(Dtype("<u4") == typeid(unsigned));
        REQUIRE(Dtype("<i4") == typeid(int32_t));
        // REQUIRE(Dtype("<i8") == typeid(long));
        REQUIRE(Dtype("<i8") == typeid(int64_t));
        REQUIRE(Dtype("<u4") == typeid(uint32_t));
        REQUIRE(Dtype("<u8") == typeid(uint64_t));
        REQUIRE(Dtype("f4") == typeid(float));
        REQUIRE(Dtype("f8") == typeid(double));
    }

    if (endian::native == endian::big) {
        REQUIRE(Dtype(">i1") == typeid(int8_t));
        REQUIRE(Dtype(">u1") == typeid(uint8_t));
        REQUIRE(Dtype(">i2") == typeid(int16_t));
        REQUIRE(Dtype(">u2") == typeid(uint16_t));
        REQUIRE(Dtype(">i4") == typeid(int));
        REQUIRE(Dtype(">u4") == typeid(unsigned));
        REQUIRE(Dtype(">i4") == typeid(int32_t));
        // REQUIRE(Dtype(">i8") == typeid(long));
        REQUIRE(Dtype(">i8") == typeid(int64_t));
        REQUIRE(Dtype(">u4") == typeid(uint32_t));
        REQUIRE(Dtype(">u8") == typeid(uint64_t));
        REQUIRE(Dtype("f4") == typeid(float));
        REQUIRE(Dtype("f8") == typeid(double));
    }
}

TEST_CASE("Construct from string with endianess") {
    // TODO! handle big endian system in test!
    REQUIRE(Dtype("<i4") == typeid(int32_t));
    REQUIRE_THROWS(Dtype(">i4") == typeid(int32_t));
}

TEST_CASE("Convert to string") {
    REQUIRE(Dtype(typeid(int)).to_string() == "<i4");
}