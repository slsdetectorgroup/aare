#include "aare/RawFile.hpp"
#include "aare/File.hpp"
#include "aare/RawMasterFile.hpp" //needed for ROI

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <filesystem>

#include "test_config.hpp"
#include "test_macros.hpp"

using aare::File;
using aare::RawFile;
using namespace aare;

TEST_CASE("Read number of frames from a jungfrau raw file", "[.integration]") {

    auto fpath =
        test_data_path() / "jungfrau" / "jungfrau_single_master_0.json";
    REQUIRE(std::filesystem::exists(fpath));

    File f(fpath, "r");
    REQUIRE(f.total_frames() == 10);
}

TEST_CASE("Read frame numbers from a jungfrau raw file", "[.integration]") {
    auto fpath =
        test_data_path() / "jungfrau" / "jungfrau_single_master_0.json";
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

TEST_CASE("Read a frame number too high throws", "[.integration]") {
    auto fpath =
        test_data_path() / "jungfrau" / "jungfrau_single_master_0.json";
    REQUIRE(std::filesystem::exists(fpath));

    File f(fpath, "r");

    // we know this file has 10 frames with frame numbers 1 to 10
    // f0 1,2,3
    // f1 4,5,6
    // f2 7,8,9
    // f3 10
    REQUIRE_THROWS(f.frame_number(10));
}

TEST_CASE("Read a frame numbers where the subfile is missing throws",
          "[.integration]") {
    auto fpath = test_data_path() / "jungfrau" /
                 "jungfrau_missing_subfile_master_0.json";
    REQUIRE(std::filesystem::exists(fpath));

    File f(fpath, "r");

    // we know this file has 10 frames with frame numbers 1 to 10
    // f0 1,2,3
    // f1 4,5,6 - but files f1-f3 are missing
    // f2 7,8,9 - gone
    // f3 10    - gone
    REQUIRE(f.frame_number(0) == 1);
    REQUIRE(f.frame_number(1) == 2);
    REQUIRE(f.frame_number(2) == 3);
    REQUIRE_THROWS(f.frame_number(4));
    REQUIRE_THROWS(f.frame_number(7));
    REQUIRE_THROWS(f.frame_number(937));
    REQUIRE_THROWS(f.frame_number(10));
}

TEST_CASE("Read data from a jungfrau 500k single port raw file",
          "[.integration]") {
    auto fpath =
        test_data_path() / "jungfrau" / "jungfrau_single_master_0.json";
    REQUIRE(std::filesystem::exists(fpath));

    File f(fpath, "r");

    // we know this file has 10 frames with pixel 0,0 being: 2123, 2051, 2109,
    // 2117, 2089, 2095, 2072, 2126, 2097, 2102
    std::vector<uint16_t> pixel_0_0 = {2123, 2051, 2109, 2117, 2089,
                                       2095, 2072, 2126, 2097, 2102};
    for (size_t i = 0; i < 10; i++) {
        auto frame = f.read_frame();
        CHECK(frame.rows() == 512);
        CHECK(frame.cols() == 1024);
        CHECK(frame.view<uint16_t>()(0, 0) == pixel_0_0[i]);
    }
}

TEST_CASE("Read frame numbers from a raw file", "[.integration]") {
    auto fpath = test_data_path() / "eiger" / "eiger_500k_16bit_master_0.json";
    REQUIRE(std::filesystem::exists(fpath));

    // we know this file has 3 frames with frame numbers 14, 15, 16
    std::vector<size_t> frame_numbers = {14, 15, 16};

    File f(fpath, "r");
    for (size_t i = 0; i < 3; i++) {
        CHECK(f.frame_number(i) == frame_numbers[i]);
    }
}

TEST_CASE("Compare reading from a numpy file with a raw file", "[.files]") {
    auto fpath_raw =
        test_data_path() / "raw/jungfrau" / "jungfrau_single_master_0.json";
    REQUIRE(std::filesystem::exists(fpath_raw));

    auto fpath_npy =
        test_data_path() / "raw/jungfrau" / "jungfrau_single_0.npy";
    REQUIRE(std::filesystem::exists(fpath_npy));

    File raw(fpath_raw, "r");
    File npy(fpath_npy, "r");

    CHECK(raw.total_frames() == 10);
    CHECK(npy.total_frames() == 10);

    for (size_t i = 0; i < 10; ++i) {
        CHECK(raw.tell() == i);
        auto raw_frame = raw.read_frame();
        auto npy_frame = npy.read_frame();
        CHECK((raw_frame.view<uint16_t>() == npy_frame.view<uint16_t>()));
    }
}

TEST_CASE("Read multipart files", "[.integration]") {
    auto fpath =
        test_data_path() / "jungfrau" / "jungfrau_double_master_0.json";
    REQUIRE(std::filesystem::exists(fpath));

    File f(fpath, "r");

    // we know this file has 10 frames check read_multiport.py for the values
    std::vector<uint16_t> pixel_0_0 = {2099, 2121, 2108, 2084, 2084,
                                       2118, 2066, 2108, 2112, 2116};
    std::vector<uint16_t> pixel_0_1 = {2842, 2796, 2865, 2798, 2805,
                                       2817, 2852, 2789, 2792, 2833};
    std::vector<uint16_t> pixel_255_1023 = {2149, 2037, 2115, 2102, 2118,
                                            2090, 2036, 2071, 2073, 2142};
    std::vector<uint16_t> pixel_511_1023 = {3231, 3169, 3167, 3162, 3168,
                                            3160, 3171, 3171, 3169, 3171};
    std::vector<uint16_t> pixel_1_0 = {2748, 2614, 2665, 2629, 2618,
                                       2630, 2631, 2634, 2577, 2598};

    for (size_t i = 0; i < 10; i++) {
        auto frame = f.read_frame();
        CHECK(frame.rows() == 512);
        CHECK(frame.cols() == 1024);
        CHECK(frame.view<uint16_t>()(0, 0) == pixel_0_0[i]);
        CHECK(frame.view<uint16_t>()(0, 1) == pixel_0_1[i]);
        CHECK(frame.view<uint16_t>()(1, 0) == pixel_1_0[i]);
        CHECK(frame.view<uint16_t>()(255, 1023) == pixel_255_1023[i]);
        CHECK(frame.view<uint16_t>()(511, 1023) == pixel_511_1023[i]);
    }
}

struct TestParameters {
    const std::string master_filename{};
    const uint8_t num_ports{};
    const DetectorGeometry geometry{};
};

TEST_CASE_PRIVATE(aare, check_find_geometry, "check find_geometry",
                  "[.integration][.files][.rawfile]") {

    auto test_parameters = GENERATE(
        TestParameters{
            "raw/jungfrau_2modules_version6.1.2/run_master_0.raw", 2,
            DetectorGeometry{1, 2, 1024, 1024, 0, 0,
                             std::vector<ModuleGeometry>{
                                 ModuleGeometry{0, 0, 512, 1024, 0, 0},
                                 ModuleGeometry{0, 512, 512, 1024, 0, 1}}}},
        TestParameters{
            "raw/eiger_1_module_version7.0.0/eiger_1mod_master_7.json", 4,
            DetectorGeometry{2, 2, 1024, 512, 0, 0,
                             std::vector<ModuleGeometry>{
                                 ModuleGeometry{0, 0, 256, 512, 0, 0},
                                 ModuleGeometry{512, 0, 256, 512, 0, 1},
                                 ModuleGeometry{0, 256, 256, 512, 1, 0},
                                 ModuleGeometry{512, 256, 256, 512, 1, 1}}}},

        TestParameters{
            "raw/jungfrau_2modules_2interfaces/run_master_0.json", 4,
            DetectorGeometry{1, 4, 1024, 1024, 0, 0,
                             std::vector<ModuleGeometry>{
                                 ModuleGeometry{0, 0, 256, 1024, 0, 0},
                                 ModuleGeometry{0, 256, 256, 1024, 1, 0},
                                 ModuleGeometry{0, 512, 256, 1024, 2, 0},
                                 ModuleGeometry{0, 768, 256, 1024, 3, 0}}}},
        TestParameters{
            "raw/eiger_quad_data/"
            "W13_vthreshscan_m21C_300V_800eV_vrpre3400_master_0.json",
            2,
            DetectorGeometry{1, 2, 512, 512, 0, 0,
                             std::vector<ModuleGeometry>{
                                 ModuleGeometry{0, 0, 256, 512, 0, 0},
                                 ModuleGeometry{0, 256, 256, 512, 1, 0}}}});

    auto fpath = test_data_path() / test_parameters.master_filename;

    REQUIRE(std::filesystem::exists(fpath));

    RawFile f(fpath, "r");

    f.m_geometry.module_pixel_0.clear();
    f.find_geometry();

    auto geometry = f.m_geometry;

    CHECK(geometry.modules_x == test_parameters.geometry.modules_x);
    CHECK(geometry.modules_y == test_parameters.geometry.modules_y);
    CHECK(geometry.pixels_x == test_parameters.geometry.pixels_x);
    CHECK(geometry.pixels_y == test_parameters.geometry.pixels_y);

    REQUIRE(geometry.module_pixel_0.size() == test_parameters.num_ports);

    // compare to data stored in header
    for (size_t i = 0; i < test_parameters.num_ports; ++i) {

        auto subfile1_path = f.master().data_fname(i, 0);
        REQUIRE(std::filesystem::exists(subfile1_path));

        auto header = RawFile::read_header(subfile1_path);

        CHECK(header.column == geometry.module_pixel_0[i].col_index);
        CHECK(header.row == geometry.module_pixel_0[i].row_index);

        CHECK(geometry.module_pixel_0[i].height ==
              test_parameters.geometry.module_pixel_0[i].height);
        CHECK(geometry.module_pixel_0[i].width ==
              test_parameters.geometry.module_pixel_0[i].width);

        CHECK(geometry.module_pixel_0[i].origin_x ==
              test_parameters.geometry.module_pixel_0[i].origin_x);
        CHECK(geometry.module_pixel_0[i].origin_y ==
              test_parameters.geometry.module_pixel_0[i].origin_y);
    }
}

} // close the namespace

TEST_CASE_PRIVATE(aare, open_multi_module_file_with_roi,
                  "Open multi module file with ROI",
                  "[.integration][.files][.rawfile]") {

    auto fpath = test_data_path() / "raw/SingleChipROI/Data_master_0.json";
    REQUIRE(std::filesystem::exists(fpath));

    RawFile f(fpath, "r");

    REQUIRE(f.master().roi().value().width() == 256);
    REQUIRE(f.master().roi().value().height() == 256);

    CHECK(f.n_modules() == 2);

    CHECK(f.n_modules_in_roi() == 1);
}
} // close namespace aare

TEST_CASE("Read file with unordered frames", "[.integration]") {
    // TODO! Better explanation and error message
    auto fpath = test_data_path() / "mythen" / "scan242_master_3.raw";
    REQUIRE(std::filesystem::exists(fpath));
    File f(fpath);
    REQUIRE_THROWS((f.read_frame()));
}
