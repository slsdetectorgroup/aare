// SPDX-License-Identifier: MPL-2.0
#include "aare/RawFile.hpp"
#include "aare/File.hpp"
#include "aare/GapPixels.hpp"
#include "aare/RawMasterFile.hpp" //needed for ROI

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>
#include <filesystem>

#include "raw_file_helpers.hpp"
#include "test_config.hpp"
#include "test_macros.hpp"

using aare::File;
using aare::RawFile;
using namespace aare;

TEST_CASE("RawFile requires padding or discarding partial frames",
          "[RawFile][frame-policy]") {
    const auto [padding, policy, supported] =
        GENERATE(table<size_t, std::string, bool>({
            {0, "nodiscard", false},
            {0, "discard", false},
            {0, "discardpartial", true},
            {1, "nodiscard", true},
            {1, "discard", true},
            {1, "discardpartial", true},
        }));
    CAPTURE(padding, policy);
    TemporaryRawFiles files;
    nlohmann::json metadata;
    std::ifstream(files.master_path()) >> metadata;
    metadata["Frame Padding"] = padding;
    metadata["Frame Discard Policy"] = policy;
    std::ofstream(files.master_path()) << metadata;

    REQUIRE(RawMasterFile(files.master_path()).frame_padding() == padding);
    if (supported) {
        RawFile reader(files.master_path());
        REQUIRE(reader.total_frames() == 2);
        auto frame = reader.read_frame();
        REQUIRE(frame.view<uint16_t>()(0, 0) == 1);
        File generic_reader(files.master_path());
        auto generic_frame = generic_reader.read_frame();
        REQUIRE(generic_frame.view<uint16_t>()(0, 0) == 1);
    } else {
        SECTION("with data subfiles") {}
        SECTION("without data subfiles") {
            std::filesystem::remove(files.data_path(0, 0));
            std::filesystem::remove(files.data_path(0, 1));
        }
        const auto path_matcher =
            Catch::Matchers::ContainsSubstring(files.master_path().string());
        const auto policy_matcher = Catch::Matchers::ContainsSubstring(
            "requires frame padding or discardpartial");
        const auto message = path_matcher && policy_matcher;
        REQUIRE_THROWS_AS(RawFile(files.master_path()), std::runtime_error);
        REQUIRE_THROWS_WITH(RawFile(files.master_path()), message);
        REQUIRE_THROWS_WITH(File(files.master_path()), message);
    }
}

TEST_CASE("RawFile read errors identify the frame index and master path",
          "[RawFile][read-errors]") {
    TemporaryRawFiles files;
    RawFile file(files.master_path());
    REQUIRE(file.frame_number(1) == 101);
    file.seek(2);
    const auto index_matcher =
        Catch::Matchers::ContainsSubstring("frame index 2");
    const auto path_matcher =
        Catch::Matchers::ContainsSubstring(files.master_path().string());
    const auto context = index_matcher && path_matcher;
    std::vector<std::byte> buffer(file.bytes_per_frame() * 2);
    DetectorHeader headers[2]{};

    SECTION("sequential frame") {
        REQUIRE_THROWS_WITH(
            file.read_frame(),
            index_matcher && path_matcher &&
                Catch::Matchers::ContainsSubstring(
                    "file contains 2 frames (indices are zero-based)") &&
                !Catch::Matchers::ContainsSubstring(".raw"));
    }
    SECTION("indexed frame at EOF") {
        REQUIRE_THROWS_WITH(file.read_frame(2), context);
    }
    SECTION("indexed frame beyond EOF") {
        REQUIRE_THROWS_WITH(
            file.read_frame(17),
            Catch::Matchers::ContainsSubstring("frame index 17") &&
                Catch::Matchers::ContainsSubstring(
                    files.master_path().string()));
    }
    SECTION("batch crosses EOF") {
        file.seek(1);
        REQUIRE_THROWS_WITH(file.read_n(2), context);
    }
    SECTION("read into buffer") {
        FileInterface &reader = file;
        REQUIRE_THROWS_WITH(reader.read_into(buffer.data()), context);
    }
    SECTION("batch into buffer") {
        file.seek(1);
        REQUIRE_THROWS_WITH(file.read_into(buffer.data(), size_t{2}), context);
    }
    SECTION("read with header") {
        REQUIRE_THROWS_WITH(file.read_into(buffer.data(), headers), context);
    }
    SECTION("batch with headers") {
        file.seek(1);
        REQUIRE_THROWS_WITH(file.read_into(buffer.data(), 2, headers), context);
    }
    SECTION("single ROI") { REQUIRE_THROWS_WITH(file.read_roi(0), context); }
    SECTION("all ROIs") { REQUIRE_THROWS_WITH(file.read_rois(), context); }
    SECTION("ROI into buffer") {
        REQUIRE_THROWS_WITH(file.read_roi_into(buffer.data(), 0, 2), context);
    }
    SECTION("ROI batch") {
        file.seek(1);
        REQUIRE_THROWS_WITH(file.read_n_with_roi(2, 0), context);
    }
    SECTION("frame number") {
        REQUIRE_THROWS_WITH(file.frame_number(2), context);
    }
    SECTION("generic File") {
        File reader(files.master_path());
        REQUIRE_THROWS_WITH(reader.read_frame(2), context);
    }
}

TEST_CASE("RawFile synchronization errors retain the requested index",
          "[RawFile][read-errors]") {
    TemporaryRawFiles files(2);
    const size_t lagging_module = GENERATE(0, 1);
    {
        std::fstream output(files.data_path(1 - lagging_module, 1),
                            std::ios::binary | std::ios::in | std::ios::out);
        DetectorHeader header{};
        header.frameNumber = 102;
        output.write(reinterpret_cast<const char *>(&header), sizeof(header));
    }
    RawFile file(files.master_path());
    REQUIRE_THROWS_WITH(
        file.read_frame(1),
        Catch::Matchers::ContainsSubstring("frame index 1") &&
            Catch::Matchers::ContainsSubstring(files.master_path().string()) &&
            Catch::Matchers::ContainsSubstring(
                files.data_path(lagging_module, 1).string()) &&
            Catch::Matchers::ContainsSubstring("synchroniz"));
}

TEST_CASE("RawFile reports the failing data file and requested frame",
          "[RawFile][read-errors]") {
    TemporaryRawFiles files;
    RawFile file(files.master_path());
    std::filesystem::resize_file(files.data_path(0, 1), sizeof(DetectorHeader));
    REQUIRE_THROWS_WITH(
        file.read_frame(1),
        Catch::Matchers::ContainsSubstring("frame index 1") &&
            !Catch::Matchers::ContainsSubstring(files.master_path().string()) &&
            Catch::Matchers::ContainsSubstring(
                files.data_path(0, 1).string()) &&
            Catch::Matchers::ContainsSubstring("End of file"));
}

TEST_CASE("RawFile bounds use the shortest subfile",
          "[RawFile][read-errors][frame-count]") {
    TemporaryRawFiles files(2);
    const size_t short_module = GENERATE(0, 1);
    std::filesystem::remove(files.data_path(short_module, 1));
    RawFile file(files.master_path());
    REQUIRE(file.total_frames() == 1);
    REQUIRE(file.master().frames_in_file() == 2);
    REQUIRE_NOTHROW(file.read_frame(0));
    const auto expected = Catch::Matchers::ContainsSubstring(
        "Error reading frame index 1 from file '" +
        files.master_path().string() + "':");

    SECTION("indexed frame") {
        REQUIRE_THROWS_WITH(file.read_frame(1), expected);
    }
    SECTION("batch") {
        file.seek(0);
        REQUIRE_THROWS_WITH(file.read_n(2), expected);
    }
    SECTION("ROI") {
        file.seek(1);
        REQUIRE_THROWS_WITH(file.read_roi(0), expected);
    }
    SECTION("generic File") {
        File reader(files.master_path());
        REQUIRE_THROWS_WITH(reader.read_frame(1), expected);
    }
    SECTION("frame number") {
        REQUIRE_THROWS_WITH(file.frame_number(1), expected);
    }
}

TEST_CASE("RawFile frame count comes from the complete subfile series",
          "[RawFile][frame-count]") {
    TemporaryRawFiles files;
    const size_t master_frames = GENERATE(0, 1, 2, 5);
    auto metadata = nlohmann::json::parse(std::ifstream(files.master_path()));
    metadata["Frames in File"] = master_frames;
    metadata["Total Frames"] = 1000;
    std::ofstream(files.master_path()) << metadata;

    RawFile file(files.master_path());
    REQUIRE(file.total_frames() == 2);
    REQUIRE(file.master().frames_in_file() == master_frames);
    REQUIRE(file.master().total_frames_expected() == 1000);
    REQUIRE(file.frame_number(1) == 101);
    REQUIRE_NOTHROW(file.read_frame(1));
    REQUIRE_THROWS(file.frame_number(2));
    REQUIRE_THROWS(file.read_frame(2));

    File reader(files.master_path());
    REQUIRE(reader.total_frames() == 2);
    REQUIRE(reader.frame_number(1) == 101);
    REQUIRE_NOTHROW(reader.read_frame(1));
}

TEST_CASE("RawFile assembles every module layout", "[RawFile][geometry]") {
    const auto [layout_rows, layout_cols] = GENERATE(table<uint32_t, uint32_t>({
        {1, 1},
        {2, 1},
        {1, 2},
        {2, 2},
        {3, 2},
    }));
    CAPTURE(layout_rows, layout_cols);
    TemporaryRawFiles files(xy{layout_rows, layout_cols});
    const size_t rows = TemporaryRawFiles::module_rows * layout_rows;
    const size_t cols = TemporaryRawFiles::module_cols * layout_cols;

    auto check = [&](NDView<uint16_t, 2> image) {
        REQUIRE(image.shape(0) == static_cast<ssize_t>(rows));
        REQUIRE(image.shape(1) == static_cast<ssize_t>(cols));
        for (size_t row = 0; row < rows; ++row) {
            for (size_t col = 0; col < cols; ++col) {
                CAPTURE(row, col);
                REQUIRE(image(row, col) == files.expected(row, col));
            }
        }
    };

    RawFile file(files.master_path());
    REQUIRE(file.rows() == rows);
    REQUIRE(file.cols() == cols);
    REQUIRE(file.n_modules() == files.n_modules());
    REQUIRE(file.bytes_per_frame() == rows * cols * sizeof(uint16_t));

    SECTION("read_frame") {
        auto frame = file.read_frame();
        check(frame.view<uint16_t>());
    }
    SECTION("read_into with headers") {
        NDArray<uint16_t, 2> image(
            {static_cast<ssize_t>(rows), static_cast<ssize_t>(cols)});
        std::vector<DetectorHeader> headers(files.n_modules());
        file.seek(1);
        file.read_into(reinterpret_cast<std::byte *>(image.data()),
                       headers.data());
        check(image.view());
        for (size_t module = 0; module < files.n_modules(); ++module) {
            CAPTURE(module);
            REQUIRE(headers[module].frameNumber == 101);
            REQUIRE(headers[module].row == module % layout_rows);
            REQUIRE(headers[module].column == module / layout_rows);
        }
    }
    SECTION("read_n") {
        auto frames = file.read_n(2);
        REQUIRE(frames.size() == 2);
        for (auto &frame : frames) {
            check(frame.view<uint16_t>());
        }
    }
    SECTION("generic File") {
        File reader(files.master_path());
        auto frame = reader.read_frame(1);
        check(frame.view<uint16_t>());
    }
}

namespace {

/// Compare a frame read with gap pixels against the free function applied
/// to the same frame read without gaps.
template <typename T>
bool matches_gap_oracle(Frame &gapped, Frame &plain, const ROI &roi,
                        DetectorType detector, bool quad,
                        const GapPixels &gaps) {
    auto expected = insert_gap_pixels(NDView<const T, 2>(plain.view<T>()), roi,
                                      detector, quad, gaps);
    auto actual = gapped.view<T>();
    if (expected.shape() != actual.shape()) {
        UNSCOPED_INFO("shape " << actual.shape(0) << "x" << actual.shape(1)
                               << " expected " << expected.shape(0) << "x"
                               << expected.shape(1));
        return false;
    }
    for (ssize_t row = 0; row < expected.shape(0); ++row) {
        for (ssize_t col = 0; col < expected.shape(1); ++col) {
            if (expected(row, col) != actual(row, col)) {
                UNSCOPED_INFO("pixel (" << row << ", " << col << ") "
                                        << actual(row, col) << " expected "
                                        << expected(row, col));
                return false;
            }
        }
    }
    return true;
}

bool matches_gap_oracle(Frame &gapped, Frame &plain, const ROI &roi,
                        DetectorType detector, bool quad,
                        const GapPixels &gaps) {
    switch (plain.bitdepth()) {
    case 8:
        return matches_gap_oracle<uint8_t>(gapped, plain, roi, detector, quad,
                                           gaps);
    case 16:
        return matches_gap_oracle<uint16_t>(gapped, plain, roi, detector, quad,
                                            gaps);
    case 32:
        return matches_gap_oracle<uint32_t>(gapped, plain, roi, detector, quad,
                                            gaps);
    default:
        return false;
    }
}

} // namespace

TEST_CASE("RawFile inserts gap pixels into synthetic Jungfrau modules",
          "[RawFile][GapPixels]") {
    const auto [layout_rows, layout_cols] = GENERATE(table<uint32_t, uint32_t>({
        {1, 1},
        {2, 1},
        {1, 2},
    }));
    CAPTURE(layout_rows, layout_cols);
    TemporaryRawFiles files(xy{layout_rows, layout_cols}, xy{512, 1024});
    const ROI detector{0, static_cast<ssize_t>(files.cols()), 0,
                       static_cast<ssize_t>(files.rows())};

    GapPixels gaps;
    SECTION("defaults") {}
    SECTION("module gaps and fill value") {
        gaps.module_gaps = ModuleGaps{8, 36};
        gaps.fill_value = 7;
    }
    const auto layout = detail::gap_layout(DetectorType::Jungfrau, false, gaps);
    const auto shape = detail::gapped_shape(detector, layout);
    const auto fill = static_cast<uint16_t>(gaps.fill_value);

    RawFile file(files.master_path(), "r", gaps);
    REQUIRE(file.gap_pixels() == gaps);
    REQUIRE(file.rows() == static_cast<size_t>(shape[0]));
    REQUIRE(file.cols() == static_cast<size_t>(shape[1]));
    REQUIRE(file.pixels_per_frame() ==
            static_cast<size_t>(shape[0] * shape[1]));
    REQUIRE(file.bytes_per_frame() == file.pixels_per_frame() * 2);

    std::vector<DetectorHeader> headers(files.n_modules());
    NDArray<uint16_t, 2> image(shape);
    file.read_into(reinterpret_cast<std::byte *>(image.data()), headers.data());
    for (const auto &header : headers) {
        REQUIRE(header.frameNumber == 100);
    }

    for (size_t row = 0; row < files.rows(); ++row) {
        for (size_t col = 0; col < files.cols(); ++col) {
            const auto value =
                image(layout.y.gapped(static_cast<ssize_t>(row)),
                      layout.x.gapped(static_cast<ssize_t>(col)));
            if (value != files.expected(row, col)) {
                CAPTURE(row, col);
                REQUIRE(value == files.expected(row, col));
            }
        }
    }
    for (const auto boundary :
         layout.x.inner_boundaries(detector.xmin, detector.xmax)) {
        const auto [first, last] = layout.x.gap_range(boundary, 0);
        for (ssize_t col = first; col <= last; ++col) {
            for (ssize_t row = 0; row < shape[0]; ++row) {
                if (image(row, col) != fill) {
                    CAPTURE(row, col);
                    REQUIRE(image(row, col) == fill);
                }
            }
        }
    }
    for (const auto boundary :
         layout.y.inner_boundaries(detector.ymin, detector.ymax)) {
        const auto [first, last] = layout.y.gap_range(boundary, 0);
        for (ssize_t row = first; row <= last; ++row) {
            for (ssize_t col = 0; col < shape[1]; ++col) {
                if (image(row, col) != fill) {
                    CAPTURE(row, col);
                    REQUIRE(image(row, col) == fill);
                }
            }
        }
    }

    auto frame = file.read_frame();
    REQUIRE(frame.rows() == static_cast<size_t>(shape[0]));
    REQUIRE(frame.cols() == static_cast<size_t>(shape[1]));
    REQUIRE(frame.view<uint16_t>() == image.view());
}

TEST_CASE("RawFile splits counts like the free function",
          "[RawFile][GapPixels]") {
    TemporaryRawFiles files(xy{1, 2}, xy{512, 1024});
    GapPixels gaps;
    gaps.split_counts = true;
    gaps.seed = 7;
    RawFile gapped(files.master_path(), "r", gaps);
    RawFile plain(files.master_path());
    auto gapped_frame = gapped.read_frame();
    auto plain_frame = plain.read_frame();
    const ROI detector{0, 2048, 0, 512};
    REQUIRE(matches_gap_oracle(gapped_frame, plain_frame, detector,
                               DetectorType::Jungfrau, false, gaps));

    // the next frame continues the generator, so the oracle for frame 1 is
    // the free function applied after the same number of draws
    auto frames = gapped.read_n(1);
    auto second_plain = plain.read_frame();
    auto expected = insert_gap_pixels(
        NDView<const uint16_t, 2>(second_plain.view<uint16_t>()), detector,
        DetectorType::Jungfrau, false, gaps);
    double gapped_total = 0;
    double expected_total = 0;
    auto view = frames[0].view<uint16_t>();
    for (ssize_t row = 0; row < expected.shape(0); ++row) {
        for (ssize_t col = 0; col < expected.shape(1); ++col) {
            gapped_total += view(row, col);
            expected_total += expected(row, col);
        }
    }
    REQUIRE(gapped_total == expected_total);
}

TEST_CASE("RawFile rejects gap pixels for unsupported detectors",
          "[RawFile][GapPixels]") {
    TemporaryRawFiles files;
    auto metadata = nlohmann::json::parse(std::ifstream(files.master_path()));
    metadata["Detector Type"] = "Moench";
    std::ofstream(files.master_path()) << metadata;
    REQUIRE_NOTHROW(RawFile(files.master_path()));
    REQUIRE_THROWS_AS(RawFile(files.master_path(), "r", GapPixels{}),
                      std::invalid_argument);
}

TEST_CASE("Gap pixels on recorded Jungfrau and Eiger files",
          "[.with-data][RawFile][GapPixels]") {
    struct Case {
        const char *path;
        std::array<ssize_t, 2> shape;
        std::optional<ModuleGaps> module_gaps;
    };
    const auto test = GENERATE(values<Case>({
        {"raw/jungfrau/jungfrau_single_master_0.json", {514, 1030}, {}},
        {"raw/jungfrau/jungfrau_double_master_0.json", {514, 1030}, {}},
        {"raw/eiger/Lab6_20500eV_2deg_20240629_master_7.json", {514, 1030}, {}},
        {"raw/eiger/eiger_500k_16bit_master_0.json", {514, 1030}, {}},
        {"raw/eiger_quad_data/"
         "W13_vrpreampscan_m21C_300V_800eV_vthre2000_master_0.json",
         {514, 514},
         {}},
        {"raw/virtual/920/jungfrau/two_modules_master_0.json",
         {1028, 1030},
         {}},
        {"raw/virtual/920/jungfrau/two_modules_master_0.json",
         {1064, 1030},
         ModuleGaps{8, 36}},
        {"raw/ROITestData/MultipleChipROI/run_master_2.json", {301, 103}, {}},
        {"raw/ROITestData/MultipleChipROI/run_master_2.json",
         {337, 103},
         ModuleGaps{8, 36}},
        {"raw/virtual/920/jungfrau/"
         "single_module_roi_512_767_0_255_master_0.json",
         {256, 256},
         {}},
    }));
    CAPTURE(test.path);
    const auto fpath = test_data_path() / test.path;
    REQUIRE(std::filesystem::exists(fpath));

    GapPixels gaps;
    gaps.module_gaps = test.module_gaps;
    gaps.fill_value = 3;
    RawFile gapped(fpath, "r", gaps);
    RawFile plain(fpath);
    REQUIRE(gapped.rows() == static_cast<size_t>(test.shape[0]));
    REQUIRE(gapped.cols() == static_cast<size_t>(test.shape[1]));
    REQUIRE(gapped.bytes_per_frame() ==
            gapped.pixels_per_frame() * gapped.bytes_per_pixel());

    auto gapped_frame = gapped.read_frame();
    auto plain_frame = plain.read_frame();
    REQUIRE(gapped_frame.rows() == static_cast<size_t>(test.shape[0]));
    REQUIRE(gapped_frame.cols() == static_cast<size_t>(test.shape[1]));
    REQUIRE(matches_gap_oracle(gapped_frame, plain_frame, plain.master().roi(),
                               plain.detector_type(),
                               plain.master().quad() == 1, gaps));
}

TEST_CASE("Split counts on a recorded Jungfrau file",
          "[.with-data][RawFile][GapPixels]") {
    const auto fpath =
        test_data_path() / "raw/jungfrau/jungfrau_single_master_0.json";
    GapPixels gaps;
    gaps.split_counts = true;
    gaps.seed = 2024;
    RawFile gapped(fpath, "r", gaps);
    RawFile plain(fpath);
    auto gapped_frame = gapped.read_frame();
    auto plain_frame = plain.read_frame();
    REQUIRE(matches_gap_oracle(gapped_frame, plain_frame, plain.master().roi(),
                               DetectorType::Jungfrau, false, gaps));
}

TEST_CASE("Gap pixels on a file with two ROIs",
          "[.with-data][RawFile][GapPixels]") {
    const auto fpath =
        test_data_path() / "raw/ROITestData/MultipleROIs/run_master_0.json";
    GapPixels gaps;
    RawFile gapped(fpath, "r", gaps);
    RawFile plain(fpath);
    REQUIRE(gapped.num_rois() == 2);
    // x 200..300 holds the chip boundary at 256; y 100..400 holds the one at
    // 256 while y 600..700 holds none
    REQUIRE(gapped.rows(0) == 303);
    REQUIRE(gapped.cols(0) == 103);
    REQUIRE(gapped.rows(1) == 101);
    REQUIRE(gapped.cols(1) == 103);

    auto gapped_frames = gapped.read_rois();
    auto plain_frames = plain.read_rois();
    const auto rois = plain.master().rois();
    REQUIRE(gapped_frames.size() == 2);
    for (size_t index = 0; index < 2; ++index) {
        CAPTURE(index);
        REQUIRE(matches_gap_oracle(gapped_frames[index], plain_frames[index],
                                   rois[index], DetectorType::Jungfrau, false,
                                   gaps));
    }
}

TEST_CASE("Read number of frames from a jungfrau raw file",
          "[.with-data][RawFile]") {

    auto fpath =
        test_data_path() / "raw/jungfrau/jungfrau_single_master_0.json";
    REQUIRE(std::filesystem::exists(fpath));

    File f(fpath, "r");
    REQUIRE(f.total_frames() == 10);
}

TEST_CASE("Read frame numbers from a jungfrau raw file",
          "[.with-data][RawFile]") {
    auto fpath =
        test_data_path() / "raw/jungfrau/jungfrau_single_master_0.json";
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

TEST_CASE("Read a frame number too high throws", "[.with-data][RawFile]") {
    auto fpath =
        test_data_path() / "raw/jungfrau/jungfrau_single_master_0.json";
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
          "[.with-data][RawFile]") {
    auto fpath = test_data_path() / "raw/jungfrau" /
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
          "[.with-data][RawFile]") {
    auto fpath =
        test_data_path() / "raw/jungfrau/jungfrau_single_master_0.json";
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

TEST_CASE("Read frame numbers from a raw file", "[.with-data][RawFile]") {
    auto fpath =
        test_data_path() / "raw/eiger" / "eiger_500k_16bit_master_0.json";
    REQUIRE(std::filesystem::exists(fpath));

    // we know this file has 3 frames with frame numbers 14, 15, 16
    std::vector<size_t> frame_numbers = {14, 15, 16};

    File f(fpath, "r");
    for (size_t i = 0; i < 3; i++) {
        CHECK(f.frame_number(i) == frame_numbers[i]);
    }
}

TEST_CASE("Compare reading from a numpy file with a raw file",
          "[.with-data][RawFile]") {

    SECTION("jungfrau data") {
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

    SECTION("eiger quad data") {
        auto fpath_raw =
            test_data_path() / "raw/eiger_quad_data" /
            "W13_vrpreampscan_m21C_300V_800eV_vthre2000_master_0.json";
        REQUIRE(std::filesystem::exists(fpath_raw));

        auto fpath_npy = test_data_path() / "raw/eiger_quad_data" /
                         "W13_vrpreampscan_m21C_300V_800eV_vthre2000.npy";
        REQUIRE(std::filesystem::exists(fpath_npy));

        File raw(fpath_raw, "r");
        File npy(fpath_npy, "r");

        raw.seek(20);
        auto raw_frame = raw.read_frame();

        auto npy_frame = npy.read_frame();
        CHECK(raw_frame.rows() == 512);
        CHECK(raw_frame.cols() == 512);
        CHECK(npy_frame.rows() == 512);
        CHECK(npy_frame.cols() == 512);

        CHECK((raw_frame.view<uint32_t>() == npy_frame.view<uint32_t>()));
    }
    SECTION("eiger data") {
        auto fpath_raw = test_data_path() / "raw/eiger" /
                         "Lab6_20500eV_2deg_20240629_master_7.json";
        REQUIRE(std::filesystem::exists(fpath_raw));

        auto fpath_npy =
            test_data_path() / "raw/eiger" / "Lab6_20500eV_2deg_20240629_7.npy";
        REQUIRE(std::filesystem::exists(fpath_npy));

        File raw(fpath_raw, "r");
        File npy(fpath_npy, "r");

        auto raw_frame = raw.read_frame();

        auto npy_frame = npy.read_frame();
        CHECK((raw_frame.view<uint32_t>() == npy_frame.view<uint32_t>()));
    }
}

TEST_CASE("Read multipart files", "[.with-data][RawFile]") {
    auto fpath =
        test_data_path() / "raw/jungfrau" / "jungfrau_double_master_0.json";
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
    const size_t modules_x{};
    const size_t modules_y{};
    const size_t pixels_x{};
    const size_t pixels_y{};
    std::vector<ModuleGeometry> module_geometries{};
};

TEST_CASE("check find_geometry", "[.with-data][RawFile]") {

    auto test_parameters = GENERATE(
        TestParameters{"raw/jungfrau_2modules_version6.1.2/run_master_0.raw", 2,
                       1, 2, 1024, 1024,
                       std::vector<ModuleGeometry>{
                           ModuleGeometry{0, 0, 512, 1024, 0, 0},
                           ModuleGeometry{0, 512, 512, 1024, 0, 1}}},
        TestParameters{
            "raw/eiger_1_module_version7.0.0/eiger_1mod_master_7.json", 4, 2, 2,
            1024, 512,
            std::vector<ModuleGeometry>{
                ModuleGeometry{0, 0, 256, 512, 0, 0},
                ModuleGeometry{512, 0, 256, 512, 0, 1},
                ModuleGeometry{0, 256, 256, 512, 1, 0},
                ModuleGeometry{512, 256, 256, 512, 1, 1}}},

        TestParameters{"raw/jungfrau_2modules_2interfaces/run_master_0.json", 4,
                       1, 4, 1024, 1024,
                       std::vector<ModuleGeometry>{
                           ModuleGeometry{0, 0, 256, 1024, 0, 0},
                           ModuleGeometry{0, 256, 256, 1024, 1, 0},
                           ModuleGeometry{0, 512, 256, 1024, 2, 0},
                           ModuleGeometry{0, 768, 256, 1024, 3, 0}}},
        TestParameters{
            "raw/eiger_quad_data/"
            "W13_vthreshscan_m21C_300V_800eV_vrpre3400_master_0.json",
            2, 1, 2, 512, 512,
            std::vector<ModuleGeometry>{ModuleGeometry{0, 256, 256, 512, 1, 0},
                                        ModuleGeometry{0, 0, 256, 512, 0, 0}}});

    auto fpath = test_data_path() / test_parameters.master_filename;

    REQUIRE(std::filesystem::exists(fpath));

    RawMasterFile master_file(fpath);

    auto geometry = DetectorGeometry(
        master_file.detector_layout(), master_file.pixels_x(),
        master_file.pixels_y(), master_file.udp_interfaces_per_module(),
        master_file.quad());

    CHECK(geometry.modules_x() == test_parameters.modules_x);
    CHECK(geometry.modules_y() == test_parameters.modules_y);
    CHECK(geometry.pixels_x() == test_parameters.pixels_x);
    CHECK(geometry.pixels_y() == test_parameters.pixels_y);

    REQUIRE(geometry.get_module_geometries().size() ==
            test_parameters.num_ports);

    // compare to data stored in header
    RawFile f(fpath, "r");
    for (size_t i = 0; i < test_parameters.num_ports; ++i) {

        auto subfile1_path = f.master().data_fname(i, 0);
        REQUIRE(std::filesystem::exists(subfile1_path));

        auto header = RawFile::read_header(subfile1_path);

        CHECK(header.column == geometry.get_module_geometries(i).col_index);
        CHECK(header.row == geometry.get_module_geometries(i).row_index);

        CHECK(geometry.get_module_geometries(i).height ==
              test_parameters.module_geometries[i].height);
        CHECK(geometry.get_module_geometries(i).width ==
              test_parameters.module_geometries[i].width);

        CHECK(geometry.get_module_geometries(i).origin_x ==
              test_parameters.module_geometries[i].origin_x);
        CHECK(geometry.get_module_geometries(i).origin_y ==
              test_parameters.module_geometries[i].origin_y);
    }
}

TEST_CASE("Open multi module file with ROI",
          "[.with-data][read_rois][RawFile]") {

    auto fpath =
        test_data_path() / "raw/ROITestData/SingleChipROI/Data_master_0.json";
    REQUIRE(std::filesystem::exists(fpath));

    RawFile f(fpath, "r");

    SECTION("read 2 frames") {
        REQUIRE(f.master().roi().width() == 256);
        REQUIRE(f.master().roi().height() == 256);

        CHECK(f.n_modules() == 2);

        REQUIRE(f.n_modules_in_roi().size() == 1);
        CHECK(f.n_modules_in_roi()[0] == 1);

        auto frames = f.read_n(2);

        CHECK(frames.size() == 2);

        CHECK(frames[0].rows() == 256);
        CHECK(frames[1].cols() == 256);
    }
}

TEST_CASE("Open multi module file with ROI spanning over multiple modules",
          "[.with-data][read_rois][RawFile]") {

    auto fpath =
        test_data_path() / "raw/ROITestData/MultipleChipROI/run_master_2.json";
    REQUIRE(std::filesystem::exists(fpath));

    RawFile f(fpath, "r");

    CHECK(f.n_modules() == 2);
    CHECK(f.n_modules_in_roi().size() == 1);
    CHECK(f.n_modules_in_roi()[0] == 2);

    auto frame = f.read_frame();

    CHECK(frame.rows() == 301);
    CHECK(frame.cols() == 101);

    CHECK(f.rows() == 301);
    CHECK(f.cols() == 101);
}

TEST_CASE("Open multi module file with two ROIs",
          "[.with-data][read_rois][RawFile]") {

    auto fpath =
        test_data_path() / "raw/ROITestData/MultipleROIs/run_master_0.json";
    REQUIRE(std::filesystem::exists(fpath));

    RawFile f(fpath, "r");

    CHECK(f.n_modules() == 2);
    REQUIRE(f.n_modules_in_roi().size() == 2);
    CHECK(f.n_modules_in_roi()[0] == 1);
    CHECK(f.n_modules_in_roi()[1] == 1);

    CHECK_THROWS(f.read_frame());

    CHECK(f.tell() == 0);

    auto frame = f.read_rois();

    CHECK(f.tell() == 1);

    REQUIRE(frame.size() == 2);

    CHECK(frame[0].rows() == 301);
    CHECK(frame[0].cols() == 101);
    CHECK(frame[1].rows() == 101);
    CHECK(frame[1].cols() == 101);

    CHECK_THROWS(f.rows());
    CHECK(f.rows(0) == 301);

    auto frames = f.read_n_with_roi(2, 1);
    REQUIRE(frames.size() == 2);
    CHECK(f.tell() == 3);
    for (const auto &frm : frames) {
        CHECK(frm.rows() == 101);
        CHECK(frm.cols() == 101);
    }
}

TEST_CASE("Read file with unordered frames", "[.with-data][RawFile]") {
    // TODO! Better explanation and error message
    auto fpath = test_data_path() / "raw/mythen/scan242_master_3.raw";
    REQUIRE(std::filesystem::exists(fpath));
    File f(fpath);
    REQUIRE_THROWS((f.read_frame()));
}

TEST_CASE("Read Mythenframe", "[.with-data][RawFile]") {
    auto fpath = test_data_path() / "raw/newmythen03/run_2_master_1.json";
    REQUIRE(std::filesystem::exists(fpath));
    RawFile f(fpath);
    REQUIRE(f.master().roi().width() == 2560);
    REQUIRE(f.master().roi().height() == 1);
    auto frame = f.read_frame();
    REQUIRE(frame.cols() == 2560);
}

TEST_CASE("Read Jungfrau frame with disabled UDP ports",
          "[.with-data][RawFile][disabled_udp_ports]") {
    SECTION("disabled top port") {
        auto fpath = test_data_path() / "raw/jungfrau" /
                     "2_interfaces_top_disabled_master_6.json";
        REQUIRE(std::filesystem::exists(fpath));
        RawFile f(fpath);
        REQUIRE(f.master().disabled_udp_ports() == std::vector<size_t>{1});
        REQUIRE(f.master().udp_port_types().value() ==
                std::vector<UDPPortPosition>{UDPPortPosition::BOTTOM,
                                             UDPPortPosition::TOP});
        auto frame = f.read_frame();
        REQUIRE(frame.cols() == 1024);
        REQUIRE(frame.rows() == 256);

        auto rois = f.master().rois();

        REQUIRE(rois.size() == 1);
        REQUIRE(rois[0] == ROI{0, 1024, 0, 256});
    }

    SECTION("disabled bottom port") {
        auto fpath = test_data_path() / "raw/jungfrau" /
                     "2_interfaces_bottom_port_disabled_master_0.json";
        REQUIRE(std::filesystem::exists(fpath));
        RawFile f(fpath);
        REQUIRE(f.master().disabled_udp_ports() == std::vector<size_t>{0});
        REQUIRE(f.master().udp_port_types().value() ==
                std::vector<UDPPortPosition>{UDPPortPosition::BOTTOM,
                                             UDPPortPosition::TOP});
        auto frame = f.read_frame();
        REQUIRE(frame.cols() == 1024);
        REQUIRE(frame.rows() == 256);
        auto rois = f.master().rois();
        REQUIRE(rois.size() == 1);
        REQUIRE(rois[0] == ROI{0, 1024, 256, 512});
    }
    SECTION("2 modules - top ports disabled") {
        auto fpath = test_data_path() / "raw/jungfrau" /
                     "2_modules_2_interfaces_top_ports_disabled_master_2.json";
        REQUIRE(std::filesystem::exists(fpath));
        RawFile f(fpath);
        REQUIRE(f.master().disabled_udp_ports() == std::vector<size_t>{1, 3});
        REQUIRE(f.master().udp_port_types().value() ==
                std::vector<UDPPortPosition>{UDPPortPosition::BOTTOM,
                                             UDPPortPosition::TOP});
        REQUIRE_THROWS_WITH(
            f.read_frame(),
            Catch::Matchers::ContainsSubstring(
                "Multiple ROIs present in file. Use read_ROIs() "
                "instead")); // cannot read frame
                             // because multiple rois

        auto frame = f.read_rois();

        REQUIRE(frame.size() == 2);
        REQUIRE(frame[0].cols() == 1024);
        REQUIRE(frame[0].rows() == 256);
        REQUIRE(frame[1].cols() == 1024);
        REQUIRE(frame[1].rows() == 256);
        auto rois = f.master().rois();
        REQUIRE(rois.size() == 2);
        REQUIRE(rois[0] == ROI{0, 1024, 0, 256});
        REQUIRE(rois[1] == ROI{0, 1024, 512, 768});
    }
    SECTION("2 modules - top ports disabled - bottom port disabled") {
        auto fpath = test_data_path() / "raw/jungfrau" /
                     "2_modules_2_interfaces_disabled_ports_master_1.json";
        REQUIRE(std::filesystem::exists(fpath));
        RawFile f(fpath);
        REQUIRE(f.master().disabled_udp_ports() == std::vector<size_t>{0, 3});
        REQUIRE(f.master().udp_port_types().value() ==
                std::vector<UDPPortPosition>{UDPPortPosition::BOTTOM,
                                             UDPPortPosition::TOP});
        auto frame = f.read_frame();
        REQUIRE(frame.cols() == 1024);
        REQUIRE(frame.rows() == 512);
        auto rois = f.master().rois();
        REQUIRE(rois.size() == 1);
        REQUIRE(rois[0] == ROI{0, 1024, 256, 768});
    }
    SECTION("4 modules- mixed ports disabled") {
        auto fpath = test_data_path() / "raw/jungfrau" /
                     "4_modules_udp_disabled_master_0.json";
        REQUIRE(std::filesystem::exists(fpath));
        RawFile f(fpath);
        REQUIRE(f.master().disabled_udp_ports() ==
                std::vector<size_t>{1, 3, 4, 6});
        REQUIRE(f.master().udp_port_types().value() ==
                std::vector<UDPPortPosition>{UDPPortPosition::BOTTOM,
                                             UDPPortPosition::TOP});
        REQUIRE_THROWS_WITH(
            f.read_frame(),
            Catch::Matchers::ContainsSubstring(
                "Multiple ROIs present in file. Use read_ROIs() "
                "instead"));
        auto frames = f.read_rois();
        REQUIRE(frames.size() == 4);
        REQUIRE(frames[0].cols() == 1024);
        REQUIRE(frames[0].rows() == 256);
        REQUIRE(frames[1].cols() == 1024);
        REQUIRE(frames[1].rows() == 256);
        REQUIRE(frames[2].cols() == 1024);
        REQUIRE(frames[2].rows() == 256);
        REQUIRE(frames[3].cols() == 1024);
        REQUIRE(frames[3].rows() == 256);
        auto rois = f.master().rois();
        REQUIRE(rois.size() == 4);
        REQUIRE(rois[0] == ROI{0, 1024, 0, 256});
        REQUIRE(rois[1] == ROI{0, 1024, 512, 768});
        REQUIRE(rois[2] == ROI{1024, 2048, 256, 512});
        REQUIRE(rois[3] == ROI{1024, 2048, 768, 1024});
    }
}

TEST_CASE("Read Moench frame with disabled UDP ports",
          "[.with-data][RawFile][disabled_udp_ports]") {
    SECTION("disabled top port") {
        auto fpath = test_data_path() / "raw/moench" /
                     "2_interfaces_top_port_disabled_master_0.json";
        REQUIRE(std::filesystem::exists(fpath));
        RawFile f(fpath);
        REQUIRE(f.master().disabled_udp_ports() == std::vector<size_t>{1});
        REQUIRE(f.master().udp_port_types().value() ==
                std::vector<UDPPortPosition>{UDPPortPosition::BOTTOM,
                                             UDPPortPosition::TOP});
        auto frame = f.read_frame();
        REQUIRE(frame.cols() == 400);
        REQUIRE(frame.rows() == 200);
        auto rois = f.master().rois();

        REQUIRE(rois.size() == 1);
        REQUIRE(rois[0] == ROI{0, 400, 0, 200});
    }

    SECTION("disabled bottom port") {
        auto fpath = test_data_path() / "raw/moench" /
                     "2_interfaces_bottom_port_disabled_master_6.json";
        REQUIRE(std::filesystem::exists(fpath));
        RawFile f(fpath);
        REQUIRE(f.master().disabled_udp_ports() == std::vector<size_t>{0});
        REQUIRE(f.master().udp_port_types().value() ==
                std::vector<UDPPortPosition>{UDPPortPosition::BOTTOM,
                                             UDPPortPosition::TOP});
        auto frame = f.read_frame();
        REQUIRE(frame.cols() == 400);
        REQUIRE(frame.rows() == 200);
        auto rois = f.master().rois();

        REQUIRE(rois.size() == 1);
        REQUIRE(rois[0] == ROI{0, 400, 200, 400});
    }
}

TEST_CASE("Read Eiger frame with disabled UDP ports",
          "[.with-data][RawFile][disabled_udp_ports]") {
    SECTION("disabled left port (bottom and top half module)") {
        auto fpath = test_data_path() / "raw/eiger" /
                     "left_udp_port_disabled_master_2.json";
        REQUIRE(std::filesystem::exists(fpath));
        RawFile f(fpath);
        REQUIRE(f.master().disabled_udp_ports() == std::vector<size_t>{0, 2});
        REQUIRE(f.master().udp_port_types().value() ==
                std::vector<UDPPortPosition>{UDPPortPosition::LEFT,
                                             UDPPortPosition::RIGHT});

        auto frame = f.read_frame();

        REQUIRE(frame.cols() == 512);
        REQUIRE(frame.rows() == 512);

        auto rois = f.master().rois();
        REQUIRE(rois.size() == 1);
        REQUIRE(rois[0] == ROI{512, 1024, 0, 512});
    }
    SECTION("disabled right port") {
        auto fpath = test_data_path() / "raw/eiger" /
                     "right_udp_port_disabled_master_3.json";
        REQUIRE(std::filesystem::exists(fpath));
        RawFile f(fpath);
        REQUIRE(f.master().disabled_udp_ports() == std::vector<size_t>{1, 3});
        REQUIRE(f.master().udp_port_types().value() ==
                std::vector<UDPPortPosition>{UDPPortPosition::LEFT,
                                             UDPPortPosition::RIGHT});
        auto frame = f.read_frame();
        REQUIRE(frame.cols() == 512);
        REQUIRE(frame.rows() == 512);
        auto rois = f.master().rois();
        REQUIRE(rois.size() == 1);
        REQUIRE(rois[0] == ROI{0, 512, 0, 512});
    }
    SECTION("2 full modules stacked vertically - right ports disabled") {
        auto fpath = test_data_path() / "raw/eiger" /
                     "2_modules_eiger_disabled_udp_port_master_0.json";

        REQUIRE(std::filesystem::exists(fpath));
        RawFile f(fpath);
        REQUIRE(f.master().disabled_udp_ports() ==
                std::vector<size_t>{1, 3, 5, 7});
        REQUIRE(f.master().udp_port_types().value() ==
                std::vector<UDPPortPosition>{UDPPortPosition::LEFT,
                                             UDPPortPosition::RIGHT});
        REQUIRE_THROWS_WITH(
            f.read_frame(),
            Catch::Matchers::ContainsSubstring(
                "Multiple ROIs present in file. Use read_ROIs() "
                "instead"));
        auto frames = f.read_rois();
        REQUIRE(frames.size() == 2);
        REQUIRE(frames[0].cols() == 512);
        REQUIRE(frames[0].rows() == 512);
        REQUIRE(frames[1].cols() == 512);
        REQUIRE(frames[1].rows() == 512);
        auto rois = f.master().rois();
        REQUIRE(rois.size() == 2);
        REQUIRE(rois[0] == ROI{0, 512, 0, 512});
        REQUIRE(rois[1] == ROI{1024, 1536, 0, 512});
    }
    SECTION("quad module - bottom port disabled") {
        auto fpath = test_data_path() / "raw/eiger" /
                     "quad_eiger_disabled_bottom_port_master_0.json";

        REQUIRE(std::filesystem::exists(fpath));
        RawFile f(fpath);
        REQUIRE(f.master().disabled_udp_ports() == std::vector<size_t>{1});
        REQUIRE(f.master().udp_port_types().value() ==
                std::vector<UDPPortPosition>{UDPPortPosition::TOP,
                                             UDPPortPosition::BOTTOM});
        auto frame = f.read_frame();
        REQUIRE(frame.cols() == 512);
        REQUIRE(frame.rows() == 256);
        auto rois = f.master().rois();
        REQUIRE(rois.size() == 1);
        REQUIRE(rois[0] == ROI{0, 512, 256, 512});
    }
    SECTION("only bottom left port disabled") {
        auto fpath = test_data_path() / "raw/eiger" /
                     "one_udp_port_disabled_master_0.json";

        REQUIRE(std::filesystem::exists(fpath));
        RawFile f(fpath);
        REQUIRE(f.master().disabled_udp_ports() == std::vector<size_t>{0});
        REQUIRE(f.master().udp_port_types().value() ==
                std::vector<UDPPortPosition>{UDPPortPosition::LEFT,
                                             UDPPortPosition::RIGHT});

        REQUIRE_THROWS_WITH(
            f.read_frame(),
            Catch::Matchers::ContainsSubstring(
                "Multiple ROIs present in file. Use read_ROIs() instead"));

        auto frame = f.read_rois();
        REQUIRE(frame.size() == 2);
        REQUIRE(frame[0].cols() == 512);
        REQUIRE(frame[0].rows() == 256);
        REQUIRE(frame[1].cols() == 1024);
        REQUIRE(frame[1].rows() == 256);
        auto rois = f.master().rois();
        REQUIRE(rois.size() == 2);

        REQUIRE(rois[0] == ROI{512, 1024, 0, 256});
        REQUIRE(rois[1] == ROI{0, 1024, 256, 512});
    }
    SECTION("No udp ports disabled") {
        auto fpath = test_data_path() /
                     "raw/eiger_virtual_500k_disabled_ports" /
                     "all_active_master_0.json";

        REQUIRE(std::filesystem::exists(fpath));
        RawFile f(fpath);

        auto disabled_udp_ports = f.master().disabled_udp_ports();
        REQUIRE(disabled_udp_ports.empty());

        auto udp_port_types = f.master().udp_port_types();
        REQUIRE(udp_port_types.has_value());
        REQUIRE(udp_port_types.value() ==
                std::vector<UDPPortPosition>{UDPPortPosition::LEFT,
                                             UDPPortPosition::RIGHT});
        REQUIRE(f.total_frames() == 5);

        auto frame = f.read_frame();
        REQUIRE(frame.cols() == 1024);
        REQUIRE(frame.rows() == 512);

        auto rois = f.master().rois();
        REQUIRE(rois.size() == 1);
        REQUIRE(rois[0] == ROI{0, 1024, 0, 512});
    }
}
