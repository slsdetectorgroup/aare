#include "aare/File.hpp"
#include "aare/utils/logger.hpp"
#include <catch2/catch_test_macros.hpp>
#include <filesystem>

#include "test_config.hpp"

using aare::File;

TEST_CASE("Read number of frames from a jungfrau raw file") {

    auto fpath = test_data_path() / "jungfrau" / "jungfrau_single_master_0.json";
    REQUIRE(std::filesystem::exists(fpath));

    File f(fpath, "r");
    REQUIRE(f.total_frames() == 10);
}

TEST_CASE("Read frame numbers from a jungfrau raw file") {
    auto fpath = test_data_path() / "jungfrau" / "jungfrau_single_master_0.json";
    REQUIRE(std::filesystem::exists(fpath));

    File f(fpath, "r");

    // we know this file has 10 frames with frame numbers 1 to 10
    // f0 1,2,3
    // f1 4,5,6
    // f2 7,8,9
    // f3 10
    for (size_t i = 0; i < 10; i++) {
        CHECK(f.frame_number(i) == i + 1);
    }
}

TEST_CASE("Read data from a jungfrau 500k single port raw file") {
    auto fpath = test_data_path() / "jungfrau" / "jungfrau_single_master_0.json";
    REQUIRE(std::filesystem::exists(fpath));

    File f(fpath, "r");

    // we know this file has 10 frames with pixel 0,0 being: 2123, 2051, 2109, 2117, 2089, 2095, 2072, 2126, 2097, 2102
    std::vector<uint16_t> pixel_0_0 = {2123, 2051, 2109, 2117, 2089, 2095, 2072, 2126, 2097, 2102};
    for (size_t i = 0; i < 10; i++) {
        auto frame = f.read();
        CHECK(frame.rows() == 512);
        CHECK(frame.cols() == 1024);
        CHECK(frame.view<uint16_t>()(0, 0) == pixel_0_0[i]);
    }
}

TEST_CASE("Read frame numbers from a raw file") {
    auto fpath = test_data_path() / "eiger" / "eiger_500k_16bit_master_0.json";
    REQUIRE(std::filesystem::exists(fpath));

    // we know this file has 3 frames with frame numbers 14, 15, 16
    std::vector<size_t> frame_numbers = {14, 15, 16};

    File f(fpath, "r");
    for (size_t i = 0; i < 3; i++) {
        CHECK(f.frame_number(i) == frame_numbers[i]);
    }
}

TEST_CASE("Compare reading from a numpy file with a raw file") {
    auto fpath_raw = test_data_path() / "jungfrau" / "jungfrau_single_master_0.json";
    REQUIRE(std::filesystem::exists(fpath_raw));

    auto fpath_npy = test_data_path() / "jungfrau" / "jungfrau_single_0.npy";
    REQUIRE(std::filesystem::exists(fpath_npy));

    File raw(fpath_raw, "r");
    File npy(fpath_npy, "r");

    CHECK(raw.total_frames() == 10);
    CHECK(npy.total_frames() == 10);

    for (size_t i = 0; i < 10; ++i) {
        auto raw_frame = raw.read();
        auto npy_frame = npy.read();
        CHECK(raw_frame.view<uint16_t>() == npy_frame.view<uint16_t>());
    }
}