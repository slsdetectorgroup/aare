#include "aare/Frame.hpp"
#include "aare/Dtype.hpp"
#include <catch2/catch_test_macros.hpp>

using namespace aare;

TEST_CASE("Construct a frame") {
    size_t rows = 10;
    size_t cols = 10;
    size_t bitdepth = 8;

    Frame frame(rows, cols, Dtype::from_bitdepth(bitdepth));

    REQUIRE(frame.rows() == rows);
    REQUIRE(frame.cols() == cols);
    REQUIRE(frame.bitdepth() == bitdepth);
    REQUIRE(frame.bytes() == rows * cols * bitdepth / 8);

    // data should be initialized to 0
    for (size_t i = 0; i < rows; i++) {
        for (size_t j = 0; j < cols; j++) {
            uint8_t *data = reinterpret_cast<uint8_t *>(frame.pixel_ptr(i, j));
            REQUIRE(data != nullptr);
            REQUIRE(*data == 0);
        }
    }
}

TEST_CASE("Set a value in a 8 bit frame") {
    size_t rows = 10;
    size_t cols = 10;
    size_t bitdepth = 8;

    Frame frame(rows, cols, Dtype::from_bitdepth(bitdepth));

    // set a value
    uint8_t value = 255;
    frame.set(5, 7, value);

    // only the value we did set should be non-zero
    for (size_t i = 0; i < rows; i++) {
        for (size_t j = 0; j < cols; j++) {
            uint8_t *data = reinterpret_cast<uint8_t *>(frame.pixel_ptr(i, j));
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
    size_t rows = 10;
    size_t cols = 10;
    size_t bitdepth = 64;

    Frame frame(rows, cols, Dtype::from_bitdepth(bitdepth));

    // set a value
    uint64_t value = 255;
    frame.set(5, 7, value);

    // only the value we did set should be non-zero
    for (size_t i = 0; i < rows; i++) {
        for (size_t j = 0; j < cols; j++) {
            uint64_t *data =
                reinterpret_cast<uint64_t *>(frame.pixel_ptr(i, j));
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
    size_t rows = 10;
    size_t cols = 10;
    size_t bitdepth = 8;

    Frame frame(rows, cols, Dtype::from_bitdepth(bitdepth));
    std::byte *data = frame.data();

    Frame frame2(std::move(frame));

    // state of the moved from object
    REQUIRE(frame.rows() == 0);
    REQUIRE(frame.cols() == 0);
    REQUIRE(frame.dtype() == Dtype(Dtype::TypeIndex::ERROR));
    REQUIRE(frame.data() == nullptr);

    // state of the moved to object
    REQUIRE(frame2.rows() == rows);
    REQUIRE(frame2.cols() == cols);
    REQUIRE(frame2.bitdepth() == bitdepth);
    REQUIRE(frame2.bytes() == rows * cols * bitdepth / 8);
    REQUIRE(frame2.data() == data);
}

TEST_CASE("Move assign a frame") {
    size_t rows = 10;
    size_t cols = 10;
    size_t bitdepth = 8;

    Frame frame(rows, cols, Dtype::from_bitdepth(bitdepth));
    std::byte *data = frame.data();

    Frame frame2(5, 5, Dtype::from_bitdepth(16));

    frame2 = std::move(frame);

    // state of the moved from object
    REQUIRE(frame.rows() == 0);
    REQUIRE(frame.cols() == 0);
    REQUIRE(frame.dtype() == Dtype(Dtype::TypeIndex::ERROR));
    REQUIRE(frame.data() == nullptr);

    // state of the moved to object
    REQUIRE(frame2.rows() == rows);
    REQUIRE(frame2.cols() == cols);
    REQUIRE(frame2.bitdepth() == bitdepth);
    REQUIRE(frame2.bytes() == rows * cols * bitdepth / 8);
    REQUIRE(frame2.data() == data);
}

TEST_CASE("test explicit copy constructor") {
    size_t rows = 10;
    size_t cols = 10;
    size_t bitdepth = 8;

    Frame frame(rows, cols, Dtype::from_bitdepth(bitdepth));
    std::byte *data = frame.data();

    Frame frame2 = frame.clone();

    // state of the original object
    REQUIRE(frame.rows() == rows);
    REQUIRE(frame.cols() == cols);
    REQUIRE(frame.bitdepth() == bitdepth);
    REQUIRE(frame.bytes() == rows * cols * bitdepth / 8);
    REQUIRE(frame.data() == data);

    // state of the copied object
    REQUIRE(frame2.rows() == rows);
    REQUIRE(frame2.cols() == cols);
    REQUIRE(frame2.bitdepth() == bitdepth);
    REQUIRE(frame2.bytes() == rows * cols * bitdepth / 8);
    REQUIRE(frame2.data() != data);
}
