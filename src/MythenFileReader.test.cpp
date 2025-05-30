/************************************************
 * @file MythenFileReader.test.cpp
 * @short test case for angle calibration class
 ***********************************************/

#include "aare/MythenFileReader.hpp"

#include <filesystem>

#include "test_config.hpp"

#include <catch2/catch_all.hpp>
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

using namespace aare;

TEST_CASE("test mythenfile_reader", "[.mythenfilereader][.files]") {

    auto fpath = test_data_path() / "AngleCalibration_Test_Data";

    REQUIRE(std::filesystem::exists(fpath));

        MythenFileReader file_reader(fpath, "ang1up_22keV_LaB60p3mm_48M_a_0");

    auto frame = file_reader.read_frame(320);

    CHECK(frame.detector_angle == 0.99955);

    CHECK(frame.channel_mask == std::array<uint8_t, 3>{0, 0, 1});

    CHECK(frame.photon_counts.size() == 61440);
}
