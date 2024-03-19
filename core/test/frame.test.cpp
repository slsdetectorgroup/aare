#include "aare/Frame.hpp"
#include "aare/DataSpan.hpp"
#include "aare/IFrame.hpp"
#include "aare/ImageData.hpp"
#include <catch2/catch_test_macros.hpp>

TEST_CASE("Frame") {
    // test constructor without allocation
    Frame<uint16_t> frame(10, 10, false);
    REQUIRE(frame.rows() == 10);
    REQUIRE(frame.cols() == 10);
    REQUIRE(frame.bitdepth() == 16);
    REQUIRE(frame._get_data() == nullptr);
    // REQUIRE_THROWS_AS(frame.get(0, 0), std::runtime_error);

    uint16_t *data = new uint16_t[100]();
    frame._set_data(data);
    REQUIRE(frame._get_data() == data);
    REQUIRE(frame.get(0, 0) == 0);
    data[99] = 1;
    REQUIRE(frame.get(9, 9) == 1);
    frame.set(9, 9, 2);
    REQUIRE(frame.get(9, 9) == 2);
    REQUIRE(data[99] == 2);

    // test constructor with allocation
    Frame<uint16_t> frame2(10, 1123);
    REQUIRE(frame2._get_data() != nullptr);

    // test memcpy constructor
    uint16_t *data2 = new uint16_t[100];
    for (int i = 0; i < 100; i++) {
        data2[i] = i;
    }
    Frame<uint16_t> frame3(reinterpret_cast<std::byte *>(data2), 10, 10);
    for (int i = 0; i < 100; i++) {
        REQUIRE(frame3.get(i / 10, i % 10) == i);
    }

    SECTION("test operator==") {
        REQUIRE((frame == frame3) == false);
        for (int i = 0; i < 100; i++) {
            frame.set(i / 10, i % 10, i);
        }
        REQUIRE((frame == frame3) == true);
    }
}

TEST_CASE("DataSpan") {
    // create frame with new so that we can delete it later
    Frame<uint16_t> *frame = new Frame<uint16_t>(10, 10);
    for (int i = 0; i < 100; i++) {
        frame->set(i / 10, i % 10, i);
    }
    SECTION("constructor"){
        std::byte *data = reinterpret_cast<std::byte *>(frame->_get_data());
        DataSpan<uint16_t> span(data, 10, 10);
        REQUIRE(span.rows() == 10);
        REQUIRE(span.cols() == 10);
        REQUIRE(span.bitdepth() == 16);
        for (int i = 0; i < 100; i++) {
            REQUIRE(span.get(i / 10, i % 10) == i);
        }
    }

    SECTION("span of a frame") {
        DataSpan<uint16_t> span(*frame);
        REQUIRE(span.rows() == 10);
        REQUIRE(span.cols() == 10);
        REQUIRE(span.bitdepth() == 16);
        for (int i = 0; i < 100; i++) {
            REQUIRE(span.get(i / 10, i % 10) == i);
        }
        // check that the span is a view of the frame
        frame->set(9, 9, 2024);
        REQUIRE(span.get(9, 9) == 2024);
        REQUIRE(frame->get(9, 9) == 2024);
        REQUIRE(span._get_data() == frame->_get_data());
    }

    // dataspan of a dataspan
    SECTION("span of a span") {
        frame->set(9, 9, 2024);
        DataSpan<uint16_t> span(*frame);
        DataSpan<uint16_t> span2(span);
        REQUIRE(span2.rows() == 10);
        REQUIRE(span2.cols() == 10);
        REQUIRE(span2.bitdepth() == 16);
        REQUIRE(span2.get(9, 9) == 2024);

        for (int i = 0; i < 99; i++) {
            REQUIRE(span2.get(i / 10, i % 10) == i);
        }

        // check that changing span2 changes span and frame
        span2.set(9, 7, 2025);
        REQUIRE(span2.get(9, 7) == 2025);
        REQUIRE(span.get(9, 7) == 2025);
        REQUIRE(frame->get(9, 7) == 2025);
        REQUIRE(span2._get_data() == frame->_get_data());
    }
    SECTION("span of image") {
        ImageData<uint16_t> image(*frame);
        DataSpan<uint16_t> span(image);
        REQUIRE(span.rows() == 10);
        REQUIRE(span.cols() == 10);
        REQUIRE(span.bitdepth() == 16);
        for (int i = 0; i < 100; i++) {
            REQUIRE(span.get(i / 10, i % 10) == i);
        }
        // check that the span is a view of the image not the frame
        span.set(9, 9, 2024);
        REQUIRE(span.get(9, 9) == 2024);
        REQUIRE(image.get(9, 9) == 2024);
        REQUIRE(frame->get(9, 9) != 2024);

        REQUIRE(span._get_data() != frame->_get_data());
        REQUIRE(span._get_data() == image._get_data());
    }
    REQUIRE_NOTHROW(delete frame);
}

TEST_CASE("ImageData") {
    SECTION("testing with the constructor") {
        uint16_t *data = new uint16_t[100];
        for (int i = 0; i < 100; i++) {
            data[i] = i;
        }
        ImageData<uint16_t> image(reinterpret_cast<std::byte *>(data), 10, 10);
        for (int i = 0; i < 100; i++) {
            REQUIRE(image.get(i / 10, i % 10) == i);
        }
        REQUIRE_NOTHROW(delete[] data);
    }
    SECTION("testing with the copy constructor") {
        Frame<uint16_t> frame(10, 10);
        for (int i = 0; i < 100; i++) {
            frame.set(i / 10, i % 10, i);
        }

        SECTION("ImageData of a Frame") {
            ImageData<uint16_t> image(frame);
            for (int i = 0; i < 100; i++) {
                REQUIRE(image.get(i / 10, i % 10) == i);
            }
            frame.set(9, 9, 2024);
            REQUIRE(image.get(9, 9) == 99);
            image.set(9, 9, 2025);
            REQUIRE(frame.get(9, 9) == 2024);
        }
        SECTION("ImageData of a DataSpan") {
            DataSpan<uint16_t> span(frame);
            ImageData<uint16_t> image(span);
            for (int i = 0; i < 100; i++) {
                REQUIRE(image.get(i / 10, i % 10) == i);
            }
            frame.set(9, 9, 2024);
            REQUIRE(image.get(9, 9) == 99);
            REQUIRE(span.get(9, 9) == 2024);

            image.set(9, 9, 2025);
            REQUIRE(frame.get(9, 9) == 2024);
            REQUIRE(span.get(9, 9) == 2024);
        }
    }
}