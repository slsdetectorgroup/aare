#include <aare/core/Frame.hpp>
#include <aare/core/NDView.hpp>
#include <catch2/catch_test_macros.hpp>
#include <cstdint>

using aare::Frame;
using aare::NDArray;
using aare::NDView;

TEST_CASE("Frame") {
    auto data = new uint16_t[100];
    for (int i = 0; i < 100; i++) {
        data[i] = i;
    }
    Frame f(reinterpret_cast<std::byte *>(data), 10, 10, 16);
    for (int i = 0; i < 100; i++) {
        REQUIRE((uint16_t)*f.get(i / 10, i % 10) == data[i]);
    }
    REQUIRE(f.rows() == 10);
    REQUIRE(f.cols() == 10);
    REQUIRE(f.bitdepth() == 16);

    uint16_t i = 44;
    f.set(0, 0, i);
    REQUIRE((uint16_t)*f.get(0, 0) == i);
    delete[] data;
}

TEST_CASE("NDView") {
    auto data = new uint16_t[100];
    for (int i = 0; i < 100; i++) {
        data[i] = i;
    }
    SECTION("constructors") {
        NDView<uint16_t, 2> ds(data, std::array<int64_t, 2>({10, 10}));
        for (int i = 0; i < 100; i++) {
            REQUIRE(ds(i / 10, i % 10) == data[i]);
        }
    }
    SECTION("from Frame") {
        Frame f(reinterpret_cast<std::byte *>(data), 10, 10, 16);
        NDView<uint16_t> ds = f.view<uint16_t>();
        for (int i = 0; i < 100; i++) {
            REQUIRE(ds(i / 10, i % 10) == data[i]);
        }

        f.set(0, 0, (uint16_t)44);
        REQUIRE((uint16_t)*f.get(0, 0) == 44); // check that set worked
        REQUIRE(ds(0, 0) == 44);               // check that ds is updated
        REQUIRE(data[0] == 0);                 // check that data is not updated
    }
    delete[] data;
}

TEST_CASE("NDArray") {
    auto data = new uint16_t[100];
    for (int i = 0; i < 100; i++) {
        data[i] = i;
    }
    SECTION("from Frame") {
        Frame f(reinterpret_cast<std::byte *>(data), 10, 10, 16);
        NDArray<uint16_t> img = f.image<uint16_t>();
        for (int i = 0; i < 100; i++) {
            REQUIRE(img(i / 10, i % 10) == data[i]);
        }

        f.set(0, 0, (uint16_t)44);
        REQUIRE((uint16_t)*f.get(0, 0) == 44); // check that set worked
        REQUIRE(img(0, 0) == 0);               // check that ds is updated
        REQUIRE(data[0] == 0);                 // check that data is not updated
    }
    delete[] data;
}