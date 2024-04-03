

#include "aare/DType.hpp"
#include <catch2/catch_test_macros.hpp>

using aare::DType;
using aare::endian;

TEST_CASE("Construct from typeid") {
    REQUIRE(DType(typeid(int)) == typeid(int));
    REQUIRE(DType(typeid(int)) != typeid(double));
}

TEST_CASE("Construct from string") {
    if (endian::native == endian::little) {
        REQUIRE(DType("<i1") == typeid(int8_t));
        REQUIRE(DType("<u1") == typeid(uint8_t));
        REQUIRE(DType("<i2") == typeid(int16_t));
        REQUIRE(DType("<u2") == typeid(uint16_t));
        REQUIRE(DType("<i4") == typeid(int));
        REQUIRE(DType("<u4") == typeid(unsigned));
        REQUIRE(DType("<i4") == typeid(int32_t));
        REQUIRE(DType("<i8") == typeid(long));
        REQUIRE(DType("<i8") == typeid(int64_t));
        REQUIRE(DType("<u4") == typeid(uint32_t));
        REQUIRE(DType("<u8") == typeid(uint64_t));
        REQUIRE(DType("f4") == typeid(float));
        REQUIRE(DType("f8") == typeid(double));
    }

    if (endian::native == endian::big) {
        REQUIRE(DType(">i1") == typeid(int8_t));
        REQUIRE(DType(">u1") == typeid(uint8_t));
        REQUIRE(DType(">i2") == typeid(int16_t));
        REQUIRE(DType(">u2") == typeid(uint16_t));
        REQUIRE(DType(">i4") == typeid(int));
        REQUIRE(DType(">u4") == typeid(unsigned));
        REQUIRE(DType(">i4") == typeid(int32_t));
        REQUIRE(DType(">i8") == typeid(long));
        REQUIRE(DType(">i8") == typeid(int64_t));
        REQUIRE(DType(">u4") == typeid(uint32_t));
        REQUIRE(DType(">u8") == typeid(uint64_t));
        REQUIRE(DType("f4") == typeid(float));
        REQUIRE(DType("f8") == typeid(double));
    }
}

TEST_CASE("Construct from string with endianess") {
    // TODO! handle big endian system in test!
    REQUIRE(DType("<i4") == typeid(int32_t));
    REQUIRE_THROWS(DType(">i4") == typeid(int32_t));
}

TEST_CASE("Convert to string") { REQUIRE(DType(typeid(int)).str() == "<i4"); }