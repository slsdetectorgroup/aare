// SPDX-License-Identifier: MPL-2.0
#include "aare/RawSubFile.hpp"
#include "aare/File.hpp"
#include "aare/NDArray.hpp"
#include "raw_file_helpers.hpp"
#include "test_config.hpp"
#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

using namespace aare;

TEST_CASE("RawSubFile errors identify the frame and actual data path",
          "[RawSubFile][read-errors]") {
    TemporaryRawFiles files;
    const size_t offset = GENERATE(0, 5);
    if (offset != 0) {
        for (size_t index = 0; index < 2; ++index) {
            std::filesystem::rename(files.data_path(0, index),
                                    files.data_path(0, offset + index));
        }
    }
    RawSubFile file(files.data_path(0, offset), DetectorType::Jungfrau, 2, 3,
                    16);
    CHECK(file.current_path() == files.data_path(0, offset));
    std::vector<std::byte> buffer(file.bytes_per_frame() * 3);
    DetectorHeader header{};
    const auto second_path = files.data_path(0, offset + 1);
    const auto index_matcher =
        Catch::Matchers::ContainsSubstring("frame index 1");
    const auto path_matcher =
        Catch::Matchers::ContainsSubstring(second_path.string());
    const auto context = index_matcher && path_matcher;

    SECTION("truncated header") {
        std::filesystem::resize_file(second_path, 0);
        file.seek(1);
        REQUIRE_THROWS_WITH(file.read_into(buffer.data(), &header), context);
    }
    SECTION("truncated pixels") {
        std::filesystem::resize_file(second_path, sizeof(DetectorHeader));
        file.seek(1);
        CHECK(file.current_path() == second_path);
        REQUIRE_THROWS_WITH(file.read_into(buffer.data()), context);
    }
    SECTION("frame number from truncated header") {
        std::filesystem::resize_file(second_path, 0);
        REQUIRE_THROWS_WITH(file.frame_number(1), context);
    }
    SECTION("missing next file") {
        std::filesystem::remove(second_path);
        REQUIRE_THROWS_WITH(file.seek(1), context);
    }
    SECTION("batch beyond EOF") {
        REQUIRE_THROWS_WITH(
            file.read_into(buffer.data(), size_t{3}),
            Catch::Matchers::ContainsSubstring("frame index 2") &&
                Catch::Matchers::ContainsSubstring(second_path.string()));
    }
    SECTION("seek beyond EOF") {
        REQUIRE_THROWS_WITH(
            file.seek(17),
            Catch::Matchers::ContainsSubstring("frame index 17") &&
                Catch::Matchers::ContainsSubstring(
                    files.data_path(0, offset).string()));
    }
}

TEST_CASE("Read frames directly from a RawSubFile", "[.with-data]") {
    auto fpath_raw =
        test_data_path() / "raw/jungfrau" / "jungfrau_single_d0_f0_0.raw";
    REQUIRE(std::filesystem::exists(fpath_raw));

    RawSubFile f(fpath_raw, DetectorType::Jungfrau, 512, 1024, 16);
    REQUIRE(f.rows() == 512);
    REQUIRE(f.cols() == 1024);
    REQUIRE(f.pixels_per_frame() == 512 * 1024);
    REQUIRE(f.bytes_per_frame() == 512 * 1024 * 2);
    REQUIRE(f.bytes_per_pixel() == 2);

    auto fpath_npy =
        test_data_path() / "raw/jungfrau" / "jungfrau_single_0.npy";
    REQUIRE(std::filesystem::exists(fpath_npy));

    // Numpy file with the same data to use as reference
    File npy(fpath_npy, "r");

    CHECK(f.frames_in_file() == 10);
    CHECK(npy.total_frames() == 10);

    DetectorHeader header{};
    NDArray<uint16_t, 2> image(
        {static_cast<ssize_t>(f.rows()), static_cast<ssize_t>(f.cols())});
    for (size_t i = 0; i < 10; ++i) {
        CHECK(f.tell() == i);
        f.read_into(image.buffer(), &header);
        auto npy_frame = npy.read_frame();
        CHECK((image.view() == npy_frame.view<uint16_t>()));
    }
}

TEST_CASE("Read frames directly from a RawSubFile starting at the second file",
          "[.with-data]") {
    // we know this file has 10 frames with frame numbers 1 to 10
    // f0 1,2,3
    // f1 4,5,6 <-- starting here
    // f2 7,8,9
    // f3 10

    auto fpath_raw =
        test_data_path() / "raw/jungfrau" / "jungfrau_single_d0_f1_0.raw";
    REQUIRE(std::filesystem::exists(fpath_raw));

    RawSubFile f(fpath_raw, DetectorType::Jungfrau, 512, 1024, 16);

    auto fpath_npy =
        test_data_path() / "raw/jungfrau" / "jungfrau_single_0.npy";
    REQUIRE(std::filesystem::exists(fpath_npy));

    // Numpy file with the same data to use as reference
    File npy(fpath_npy, "r");
    npy.seek(3);

    CHECK(f.frames_in_file() == 7);
    CHECK(npy.total_frames() == 10);

    DetectorHeader header{};
    NDArray<uint16_t, 2> image(
        {static_cast<ssize_t>(f.rows()), static_cast<ssize_t>(f.cols())});
    for (size_t i = 0; i < 7; ++i) {
        CHECK(f.tell() == i);
        f.read_into(image.buffer(), &header);
        // frame numbers start at 1 frame index at 0
        // adding 3 + 1 to verify the frame number
        CHECK(header.frameNumber == i + 4);
        auto npy_frame = npy.read_frame();
        CHECK((image.view() == npy_frame.view<uint16_t>()));
    }
}
