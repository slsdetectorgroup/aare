#include "aare/core/Frame.hpp"
#include <catch2/catch_test_macros.hpp>

using aare::Frame;

TEST_CASE("Construct a frame") {
    ssize_t rows = 10;
    ssize_t cols = 10;
    ssize_t bitdepth = 8;

    Frame frame(rows, cols, bitdepth);

    REQUIRE(frame.rows() == rows);
    REQUIRE(frame.cols() == cols);
    REQUIRE(frame.bitdepth() == bitdepth);
    REQUIRE(frame.size() == rows * cols * bitdepth / 8);

    // data should be initialized to 0
    for (int i = 0; i < rows; i++) {
        for (int j = 0; j < cols; j++) {
            uint8_t *data = (uint8_t *)frame.get(i, j);
            REQUIRE(data != nullptr);
            REQUIRE(*data == 0);
        }
    }
}

TEST_CASE("Set a value in a 8 bit frame") {
    ssize_t rows = 10;
    ssize_t cols = 10;
    ssize_t bitdepth = 8;

    Frame frame(rows, cols, bitdepth);

    // set a value
    uint8_t value = 255;
    frame.set(5, 7, value);

    // only the value we did set should be non-zero
    for (int i = 0; i < rows; i++) {
        for (int j = 0; j < cols; j++) {
            uint8_t *data = (uint8_t *)frame.get(i, j);
            REQUIRE(data != nullptr);
            if (i == 5 && j == 7) {
                REQUIRE(*data == value);
            } else {
                REQUIRE(*data == 0);
            }
        }
    }
}

TEST_CASE("Set a value in a 64 bit frame") {
    ssize_t rows = 10;
    ssize_t cols = 10;
    ssize_t bitdepth = 64;

    Frame frame(rows, cols, bitdepth);

    // set a value
    uint64_t value = 255;
    frame.set(5, 7, value);

    // only the value we did set should be non-zero
    for (int i = 0; i < rows; i++) {
        for (int j = 0; j < cols; j++) {
            uint64_t *data = (uint64_t *)frame.get(i, j);
            REQUIRE(data != nullptr);
            if (i == 5 && j == 7) {
                REQUIRE(*data == value);
            } else {
                REQUIRE(*data == 0);
            }
        }
    }
}

TEST_CASE("Move construct a frame") {
    ssize_t rows = 10;
    ssize_t cols = 10;
    ssize_t bitdepth = 8;

    Frame frame(rows, cols, bitdepth);
    std::byte *data = frame.data();

    Frame frame2(std::move(frame));

    // state of the moved from object
    REQUIRE(frame.rows() == 0);
    REQUIRE(frame.cols() == 0);
    REQUIRE(frame.bitdepth() == 0);
    REQUIRE(frame.size() == 0);
    REQUIRE(frame.data() == nullptr);

    // state of the moved to object
    REQUIRE(frame2.rows() == rows);
    REQUIRE(frame2.cols() == cols);
    REQUIRE(frame2.bitdepth() == bitdepth);
    REQUIRE(frame2.size() == rows * cols * bitdepth / 8);
    REQUIRE(frame2.data() == data);
}