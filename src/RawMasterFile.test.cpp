// SPDX-License-Identifier: MPL-2.0
#include "aare/RawMasterFile.hpp"

#include "test_config.hpp"
#include <catch2/catch_test_macros.hpp>
#include <iostream>
#include <sstream>

using namespace aare;

TEST_CASE("Parse a master file fname") {
    RawFileNameComponents m("test_master_1.json");
    REQUIRE(m.base_name() == "test");
    REQUIRE(m.ext() == ".json");
    REQUIRE(m.file_index() == 1);
    REQUIRE(m.base_path() == "");
}

TEST_CASE("Extraction of base path works") {
    RawFileNameComponents m("some/path/test_master_73.json");
    REQUIRE(m.base_name() == "test");
    REQUIRE(m.ext() == ".json");
    REQUIRE(m.file_index() == 73);
    REQUIRE(m.base_path() == "some/path");
}

TEST_CASE("Construction of master file name and data files") {
    RawFileNameComponents m("test_master_1.json");
    REQUIRE(m.master_fname() == "test_master_1.json");
    REQUIRE(m.data_fname(0, 0) == "test_d0_f0_1.raw");
    REQUIRE(m.data_fname(1, 0) == "test_d1_f0_1.raw");
    REQUIRE(m.data_fname(0, 1) == "test_d0_f1_1.raw");
    REQUIRE(m.data_fname(1, 1) == "test_d1_f1_1.raw");
}

TEST_CASE("Construction of master file name and data files using old scheme") {
    RawFileNameComponents m("test_master_1.raw");
    m.set_old_scheme(true);
    REQUIRE(m.master_fname() == "test_master_1.raw");
    REQUIRE(m.data_fname(0, 0) == "test_d0_f000000000000_1.raw");
    REQUIRE(m.data_fname(1, 0) == "test_d1_f000000000000_1.raw");
    REQUIRE(m.data_fname(0, 1) == "test_d0_f000000000001_1.raw");
    REQUIRE(m.data_fname(1, 1) == "test_d1_f000000000001_1.raw");
}

TEST_CASE("Master file name does not fit pattern") {
    REQUIRE_THROWS(RawFileNameComponents("somefile.json"));
    REQUIRE_THROWS(RawFileNameComponents("another_test_d0_f0_1.raw"));
    REQUIRE_THROWS(RawFileNameComponents("test_master_1.txt"));
}

TEST_CASE("Parse scan parameters") {
    ScanParameters s("[enabled\ndac dac 4\nstart 500\nstop 2200\nstep "
                     "5\nsettleTime 100us\n]");
    REQUIRE(s.enabled());
    REQUIRE(s.dac() == DACIndex::DAC_4);
    REQUIRE(s.start() == 500);
    REQUIRE(s.stop() == 2200);
    REQUIRE(s.step() == 5);
}

TEST_CASE("A disabled scan") {
    ScanParameters s("[disabled]");
    REQUIRE_FALSE(s.enabled());
    REQUIRE(s.dac() == DACIndex::DAC_0);
    REQUIRE(s.start() == 0);
    REQUIRE(s.stop() == 0);
    REQUIRE(s.step() == 0);
}

TEST_CASE("Parse a master file in .json format", "[.with-data]") {
    auto fpath =
        test_data_path() / "raw" / "jungfrau" / "jungfrau_single_master_0.json";
    REQUIRE(std::filesystem::exists(fpath));
    RawMasterFile f(fpath);

    // "Version": 7.2,
    REQUIRE(f.version() == "7.2");
    // "Timestamp": "Tue Feb 20 08:28:24 2024",
    // "Detector Type": "Jungfrau",
    REQUIRE(f.detector_type() == DetectorType::Jungfrau);
    // "Timing Mode": "auto",
    REQUIRE(f.timing_mode() == TimingMode::Auto);

    // "Geometry": {
    //     "x": 1,
    //     "y": 1
    // },
    REQUIRE(f.detector_layout().col == 1);
    REQUIRE(f.detector_layout().row == 1);

    // "Image Size in bytes": 1048576,
    REQUIRE(f.image_size_in_bytes() == 1048576);
    // "Pixels": {
    //     "x": 1024,
    REQUIRE(f.pixels_x() == 1024);
    //     "y": 512
    REQUIRE(f.pixels_y() == 512);
    // },

    // "Max Frames Per File": 3,
    REQUIRE(f.max_frames_per_file() == 3);

    // Jungfrau doesn't write but it is 16
    REQUIRE(f.bitdepth() == 16);

    // "Frame Discard Policy": "nodiscard",

    // "Frame Padding": 1,
    REQUIRE(f.frame_padding() == 1);
    // "Scan Parameters": "[disabled]",
    // "Total Frames": 10,
    REQUIRE(f.total_frames_expected() == 10);
    // "Receiver Roi": {
    //     "xmin": 4294967295,
    //     "xmax": 4294967295,
    //     "ymin": 4294967295,
    //     "ymax": 4294967295
    // },
    // "Exptime": "10us",
    // "Period": "1ms",
    // "Number of UDP Interfaces": 1,
    // "Number of rows": 512,
    REQUIRE(f.number_of_rows() == 512);
    // "Frames in File": 10,
    REQUIRE(f.frames_in_file() == 10);

    // TODO! Should we parse this?
    //  "Frame Header Format": {
    //      "Frame Number": "8 bytes",
    //      "SubFrame Number/ExpLength": "4 bytes",
    //      "Packet Number": "4 bytes",
    //      "Bunch ID": "8 bytes",
    //      "Timestamp": "8 bytes",
    //      "Module Id": "2 bytes",
    //      "Row": "2 bytes",
    //      "Column": "2 bytes",
    //      "Reserved": "2 bytes",
    //      "Debug": "4 bytes",
    //      "Round Robin Number": "2 bytes",
    //      "Detector Type": "1 byte",
    //      "Header Version": "1 byte",
    //      "Packets Caught Mask": "64 bytes"
    //  }
    //  }

    REQUIRE_FALSE(f.analog_samples());
    REQUIRE_FALSE(f.digital_samples());

    REQUIRE(f.get_reading_mode() == ReadoutMode::UNKNOWN);
}

TEST_CASE("Parse a master file in old .raw format",
          "[.with-data][rawmasterfile]") {
    auto fpath = test_data_path() /
                 "raw/jungfrau_2modules_version6.1.2/run_master_0.raw";
    REQUIRE(std::filesystem::exists(fpath));
    RawMasterFile f(fpath);

    CHECK(f.udp_interfaces_per_module() == xy{1, 1});
    CHECK(f.n_modules() == 2);
    CHECK(f.detector_layout().row == 2);
    CHECK(f.detector_layout().col == 1);
}

TEST_CASE("Parse a master file in .raw format", "[.with-data]") {

    auto fpath =
        test_data_path() /
        "raw/moench04/"
        "moench04_noise_200V_sto_both_100us_no_light_thresh_900_master_0.raw";
    REQUIRE(std::filesystem::exists(fpath));
    RawMasterFile f(fpath);

    // Version                    : 6.4
    REQUIRE(f.version() == "6.4");
    // TimeStamp                  : Wed Aug 31 09:08:49 2022

    // Detector Type              : ChipTestBoard
    REQUIRE(f.detector_type() == DetectorType::ChipTestBoard);
    // Timing Mode                : auto
    REQUIRE(f.timing_mode() == TimingMode::Auto);
    // Detector Layout            : [1, 1]
    REQUIRE(f.detector_layout().col == 1);
    REQUIRE(f.detector_layout().row == 1);
    // Image Size                 : 360000 bytes
    REQUIRE(f.image_size_in_bytes() == 360000);
    // Pixels                     : [96, 1]
    REQUIRE(f.pixels_x() == 96);
    REQUIRE(f.pixels_y() == 1);
    // Max Frames Per File        : 20000
    REQUIRE(f.max_frames_per_file() == 20000);
    // Frame Discard Policy       : nodiscard
    // Frame Padding              : 1
    REQUIRE(f.frame_padding() == 1);
    // Scan Parameters            : [disabled]
    // Total Frames               : 100
    REQUIRE(f.total_frames_expected() == 100);
    // Exptime                    : 100us
    REQUIRE(f.exptime() == std::chrono::microseconds(100));
    // Period                     : 4ms
    REQUIRE(f.period() == std::chrono::milliseconds(4));
    // Ten Giga                   : 1
    // ADC Mask                   : 0xffffffff
    // Analog Flag                : 1
    // Analog Samples             : 5000
    REQUIRE(f.analog_samples() == 5000);
    // Digital Flag               : 1
    // Digital Samples            : 5000
    REQUIRE(f.digital_samples() == 5000);
    // Dbit Offset                : 0
    // Dbit Bitset                : 0
    // Frames in File             : 100
    REQUIRE(f.frames_in_file() == 100);

    REQUIRE(f.get_reading_mode() == ReadoutMode::ANALOG_AND_DIGITAL);

    // #Frame Header
    // Frame Number               : 8 bytes
    // SubFrame Number/ExpLength  : 4 bytes
    // Packet Number              : 4 bytes
    // Bunch ID                   : 8 bytes
    // Timestamp                  : 8 bytes
    // Module Id                  : 2 bytes
    // Row                        : 2 bytes
    // Column                     : 2 bytes
    // Reserved                   : 2 bytes
    // Debug                      : 4 bytes
    // Round Robin Number         : 2 bytes
    // Detector Type              : 1 byte
    // Header Version             : 1 byte
    // Packets Caught Mask        : 64 bytes
}

TEST_CASE("Parse a master file in new .json format", "[.with-data]") {

    auto file_path =
        test_data_path() / "raw" / "newmythen03" / "run_87_master_0.json";
    REQUIRE(std::filesystem::exists(file_path));

    RawMasterFile f(file_path);

    // Version                    : 8.0
    REQUIRE(f.version() == "8.0");

    REQUIRE(f.detector_type() == DetectorType::Mythen3);
    // Timing Mode                : auto
    REQUIRE(f.timing_mode() == TimingMode::Auto);
    // Detector Layout            : [2, 1]
    REQUIRE(f.detector_layout().col == 2);
    REQUIRE(f.detector_layout().row == 1);
    // Image Size                 : 5120 bytes
    REQUIRE(f.image_size_in_bytes() == 5120);

    REQUIRE(f.scan_parameters().enabled() == false);
    REQUIRE(f.scan_parameters().dac() == DACIndex::DAC_0);
    REQUIRE(f.scan_parameters().start() == 0);
    REQUIRE(f.scan_parameters().stop() == 0);
    REQUIRE(f.scan_parameters().step() == 0);
    REQUIRE(f.scan_parameters().settleTime() == 0);

    auto roi = f.roi();
    REQUIRE(roi.xmin == 0);
    REQUIRE(roi.xmax == 2560);
    REQUIRE(roi.ymin == 0);
    REQUIRE(roi.ymax == 1);
}

TEST_CASE("Read eiger master file", "[.with-data]") {
    auto fpath = test_data_path() / "raw/eiger/eiger_500k_32bit_master_0.json";
    REQUIRE(std::filesystem::exists(fpath));
    RawMasterFile f(fpath);

    // {
    //     "Version": 7.2,
    REQUIRE(f.version() == "7.2");
    //     "Timestamp": "Tue Mar 26 17:24:34 2024",
    //     "Detector Type": "Eiger",
    REQUIRE(f.detector_type() == DetectorType::Eiger);
    //     "Timing Mode": "auto",
    REQUIRE(f.timing_mode() == TimingMode::Auto);
    //     "Geometry": {
    //         "x": 2,
    //         "y": 2
    //     },
    //     "Image Size in bytes": 524288,
    REQUIRE(f.image_size_in_bytes() == 524288);
    //     "Pixels": {
    //         "x": 512,
    REQUIRE(f.pixels_x() == 512);
    //         "y": 256
    REQUIRE(f.pixels_y() == 256);
    //     },
    //     "Max Frames Per File": 10000,
    REQUIRE(f.max_frames_per_file() == 10000);
    //     "Frame Discard Policy": "nodiscard",
    REQUIRE(f.frame_discard_policy() == FrameDiscardPolicy::NoDiscard);
    //     "Frame Padding": 1,
    REQUIRE(f.frame_padding() == 1);

    //     "Scan Parameters": "[disabled]",
    //     "Total Frames": 3,
    //     "Receiver Roi": {
    //         "xmin": 4294967295,
    //         "xmax": 4294967295,
    //         "ymin": 4294967295,
    //         "ymax": 4294967295
    //     },
    //     "Dynamic Range": 32,
    //     "Ten Giga": 0,
    // "Exptime": "5s",
    REQUIRE(f.exptime() == std::chrono::seconds(5));
    //     "Period": "1s",
    REQUIRE(f.period() == std::chrono::seconds(1));
    //     "Threshold Energy": -1,
    //     "Sub Exptime": "2.62144ms",
    //     "Sub Period": "2.62144ms",
    //     "Quad": 0,
    //     "Number of rows": 256,
    //     "Rate Corrections": "[0, 0]",
    //     "Frames in File": 3,
    //     "Frame Header Format": {
    //         "Frame Number": "8 bytes",
    //         "SubFrame Number/ExpLength": "4 bytes",
    //         "Packet Number": "4 bytes",
    //         "Bunch ID": "8 bytes",
    //         "Timestamp": "8 bytes",
    //         "Module Id": "2 bytes",
    //         "Row": "2 bytes",
    //         "Column": "2 bytes",
    //         "Reserved": "2 bytes",
    //         "Debug": "4 bytes",
    //         "Round Robin Number": "2 bytes",
    //         "Detector Type": "1 byte",
    //         "Header Version": "1 byte",
    //         "Packets Caught Mask": "64 bytes"
    //     }
    // }
}

TEST_CASE("Parse EIGER 7.2 master from string stream") {
    std::string master_content = R"({
    "Version": 7.2,
    "Timestamp": "Tue Mar 26 17:24:34 2024",
    "Detector Type": "Eiger",
    "Timing Mode": "auto",
    "Geometry": {
        "x": 2,
        "y": 2
    },
    "Image Size in bytes": 524288,
    "Pixels": {
        "x": 512,
        "y": 256
    },
    "Max Frames Per File": 10000,
    "Frame Discard Policy": "nodiscard",
    "Frame Padding": 1,
    "Scan Parameters": "[disabled]",
    "Total Frames": 3,
    "Receiver Roi": {
        "xmin": 4294967295,
        "xmax": 4294967295,
        "ymin": 4294967295,
        "ymax": 4294967295
    },
    "Dynamic Range": 32,
    "Ten Giga": 0,
    "Exptime": "5s",
    "Period": "1s",
    "Threshold Energy": -1,
    "Sub Exptime": "2.62144ms",
    "Sub Period": "2.62144ms",
    "Quad": 0,
    "Number of rows": 256,
    "Rate Corrections": "[0, 0]",
    "Frames in File": 3,
    "Frame Header Format": {
        "Frame Number": "8 bytes",
        "SubFrame Number/ExpLength": "4 bytes",
        "Packet Number": "4 bytes",
        "Bunch ID": "8 bytes",
        "Timestamp": "8 bytes",
        "Module Id": "2 bytes",
        "Row": "2 bytes",
        "Column": "2 bytes",
        "Reserved": "2 bytes",
        "Debug": "4 bytes",
        "Round Robin Number": "2 bytes",
        "Detector Type": "1 byte",
        "Header Version": "1 byte",
        "Packets Caught Mask": "64 bytes"
    }
})";

    std::istringstream iss(master_content);
    RawMasterFile f(iss, "test_master_0.json");

    REQUIRE(f.version() == "7.2");
    REQUIRE(f.detector_type() == DetectorType::Eiger);
    REQUIRE(f.timing_mode() == TimingMode::Auto);
    REQUIRE(f.detector_layout().col == 2);
    REQUIRE(f.detector_layout().row == 2);

    REQUIRE(f.image_size_in_bytes() == 524288);
    REQUIRE(f.pixels_x() == 512);
    REQUIRE(f.pixels_y() == 256);
    REQUIRE(f.max_frames_per_file() == 10000);
    REQUIRE(f.frame_discard_policy() == FrameDiscardPolicy::NoDiscard);
    REQUIRE(f.frame_padding() == 1);
    REQUIRE(f.total_frames_expected() == 3);

    REQUIRE(f.quad() == 0);
    REQUIRE(f.number_of_rows() == 256);
    REQUIRE(f.n_modules() == 4);

    REQUIRE(f.bitdepth() == 32);
    REQUIRE(f.frames_in_file() == 3);

    REQUIRE(f.exptime() == std::chrono::seconds(5));
    REQUIRE(f.period() == std::chrono::seconds(1));
}

TEST_CASE("Parse JUNGFRAU 7.2 master from string stream") {
    std::string master_content = R"({
    "Version": 7.2,
    "Timestamp": "Tue Feb 20 08:29:19 2024",
    "Detector Type": "Jungfrau",
    "Timing Mode": "auto",
    "Geometry": {
        "x": 1,
        "y": 2
    },
    "Image Size in bytes": 524288,
    "Pixels": {
        "x": 1024,
        "y": 256
    },
    "Max Frames Per File": 3,
    "Frame Discard Policy": "nodiscard",
    "Frame Padding": 1,
    "Scan Parameters": "[disabled]",
    "Total Frames": 10,
    "Receiver Roi": {
        "xmin": 4294967295,
        "xmax": 4294967295,
        "ymin": 4294967295,
        "ymax": 4294967295
    },
    "Exptime": "10us",
    "Period": "1ms",
    "Number of UDP Interfaces": 2,
    "Number of rows": 512,
    "Frames in File": 10,
    "Frame Header Format": {
        "Frame Number": "8 bytes",
        "SubFrame Number/ExpLength": "4 bytes",
        "Packet Number": "4 bytes",
        "Bunch ID": "8 bytes",
        "Timestamp": "8 bytes",
        "Module Id": "2 bytes",
        "Row": "2 bytes",
        "Column": "2 bytes",
        "Reserved": "2 bytes",
        "Debug": "4 bytes",
        "Round Robin Number": "2 bytes",
        "Detector Type": "1 byte",
        "Header Version": "1 byte",
        "Packets Caught Mask": "64 bytes"
    }
})";

    std::istringstream iss(master_content);
    RawMasterFile f(iss, "test_master_0.json");

    REQUIRE(f.version() == "7.2");
    REQUIRE(f.detector_type() == DetectorType::Jungfrau);
    REQUIRE(f.timing_mode() == TimingMode::Auto);
    REQUIRE(f.detector_layout().col == 1);
    REQUIRE(f.detector_layout().row == 2);
    REQUIRE(f.n_modules() == 2);
    REQUIRE(f.image_size_in_bytes() == 524288);
    REQUIRE(f.pixels_x() == 1024);
    REQUIRE(f.pixels_y() == 256);
    REQUIRE(f.max_frames_per_file() == 3);
    REQUIRE(f.bitdepth() == 16);
    REQUIRE(f.frame_discard_policy() == FrameDiscardPolicy::NoDiscard);
    REQUIRE(f.frame_padding() == 1);
    REQUIRE(f.total_frames_expected() == 10);
    REQUIRE(f.exptime() == std::chrono::microseconds(10));
    REQUIRE(f.period() == std::chrono::milliseconds(1));
    REQUIRE(f.number_of_rows() == 512);

    REQUIRE(f.frames_in_file() == 10);
    REQUIRE(f.quad() == 0);
    REQUIRE(f.n_modules() == 2);
    REQUIRE(f.udp_interfaces_per_module() == xy{2, 1});
}

TEST_CASE(
    "Parse CTB 7.2 master (SW 7.0.3) with analog samples from string stream") {
    std::string master_content = R"({
    "Version": 7.2,
    "Timestamp": "Wed Aug 19 09:45:29 2026",
    "Detector Type": "ChipTestBoard",
    "Timing Mode": "auto",
    "Geometry": {
        "x": 1,
        "y": 1
    },
    "Image Size in bytes": 192000,
    "Pixels": {
        "x": 32,
        "y": 1
    },
    "Max Frames Per File": 20000,
    "Frame Discard Policy": "nodiscard",
    "Frame Padding": 1,
    "Scan Parameters": "[disabled]",
    "Total Frames": 1,
    "Receiver Roi": {
        "xmin": 4294967295,
        "xmax": 4294967295,
        "ymin": 4294967295,
        "ymax": 4294967295
    },
    "Exptime": "0ns",
    "Period": "1ms",
    "Ten Giga": 0,
    "ADC Mask": "0xffffffff",
    "Analog Flag": 1,
    "Analog Samples": 3000,
    "Digital Flag": 0,
    "Digital Samples": 2000,
    "Dbit Offset": 0,
    "Dbit Bitset": 0,
    "Frames in File": 1,
    "Frame Header Format": {
        "Frame Number": "8 bytes",
        "SubFrame Number/ExpLength": "4 bytes",
        "Packet Number": "4 bytes",
        "Bunch ID": "8 bytes",
        "Timestamp": "8 bytes",
        "Module Id": "2 bytes",
        "Row": "2 bytes",
        "Column": "2 bytes",
        "Reserved": "2 bytes",
        "Debug": "4 bytes",
        "Round Robin Number": "2 bytes",
        "Detector Type": "1 byte",
        "Header Version": "1 byte",
        "Packets Caught Mask": "64 bytes"
    }
})";

    std::istringstream iss(master_content);
    RawMasterFile f(iss, "test_master_0.json");

    REQUIRE(f.version() == "7.2");
    REQUIRE(f.detector_type() == DetectorType::ChipTestBoard);
    REQUIRE(f.timing_mode() == TimingMode::Auto);
    REQUIRE(f.detector_layout() == xy{1, 1});
    REQUIRE(f.image_size_in_bytes() == 192000);
    REQUIRE(f.pixels_x() == 32);
    REQUIRE(f.pixels_y() == 1);
    REQUIRE(f.max_frames_per_file() == 20000);
    REQUIRE(f.frame_discard_policy() == FrameDiscardPolicy::NoDiscard);
    REQUIRE(f.frame_padding() == 1);
    REQUIRE(f.total_frames_expected() == 1);
    REQUIRE(f.exptime() == std::chrono::nanoseconds(0));
    REQUIRE(f.period() == std::chrono::milliseconds(1));
    REQUIRE(f.analog_samples() == 3000);
    REQUIRE(f.digital_samples() == std::nullopt);
    REQUIRE(f.transceiver_samples() == std::nullopt);
    REQUIRE(f.frames_in_file() == 1);
}

TEST_CASE(
    "Parse CTB 7.2 master (SW 7.0.3) with digital samples from string stream") {
    std::string master_content = R"({
    "Version": 7.2,
    "Timestamp": "Wed Aug 19 09:48:34 2026",
    "Detector Type": "ChipTestBoard",
    "Timing Mode": "auto",
    "Geometry": {
        "x": 1,
        "y": 1
    },
    "Image Size in bytes": 16000,
    "Pixels": {
        "x": 64,
        "y": 1
    },
    "Max Frames Per File": 20000,
    "Frame Discard Policy": "nodiscard",
    "Frame Padding": 1,
    "Scan Parameters": "[disabled]",
    "Total Frames": 1,
    "Receiver Roi": {
        "xmin": 4294967295,
        "xmax": 4294967295,
        "ymin": 4294967295,
        "ymax": 4294967295
    },
    "Exptime": "0ns",
    "Period": "1ms",
    "Ten Giga": 0,
    "ADC Mask": "0xffffffff",
    "Analog Flag": 0,
    "Analog Samples": 3000,
    "Digital Flag": 1,
    "Digital Samples": 2000,
    "Dbit Offset": 0,
    "Dbit Bitset": 0,
    "Frames in File": 1,
    "Frame Header Format": {
        "Frame Number": "8 bytes",
        "SubFrame Number/ExpLength": "4 bytes",
        "Packet Number": "4 bytes",
        "Bunch ID": "8 bytes",
        "Timestamp": "8 bytes",
        "Module Id": "2 bytes",
        "Row": "2 bytes",
        "Column": "2 bytes",
        "Reserved": "2 bytes",
        "Debug": "4 bytes",
        "Round Robin Number": "2 bytes",
        "Detector Type": "1 byte",
        "Header Version": "1 byte",
        "Packets Caught Mask": "64 bytes"
    }
})";

    std::istringstream iss(master_content);
    RawMasterFile f(iss, "test_master_0.json");

    REQUIRE(f.version() == "7.2");
    REQUIRE(f.detector_type() == DetectorType::ChipTestBoard);
    REQUIRE(f.timing_mode() == TimingMode::Auto);
    REQUIRE(f.detector_layout() == xy{1, 1});
    REQUIRE(f.image_size_in_bytes() == 16000);
    REQUIRE(f.pixels_x() == 64);
    REQUIRE(f.pixels_y() == 1);
    REQUIRE(f.max_frames_per_file() == 20000);
    REQUIRE(f.frame_discard_policy() == FrameDiscardPolicy::NoDiscard);
    REQUIRE(f.frame_padding() == 1);
    REQUIRE(f.total_frames_expected() == 1);
    REQUIRE(f.exptime() == std::chrono::nanoseconds(0));
    REQUIRE(f.period() == std::chrono::milliseconds(1));
    REQUIRE(f.analog_samples() == std::nullopt);
    REQUIRE(f.digital_samples() == 2000);
    REQUIRE(f.transceiver_samples() == std::nullopt);
    REQUIRE(f.frames_in_file() == 1);
}

TEST_CASE("Parse Moench 7.2 master (SW 7.0.3) from string stream") {
    std::string master_content = R"({
    "Version": 7.2,
    "Timestamp": "Wed Aug 19 10:32:06 2026",
    "Detector Type": "Moench",
    "Timing Mode": "auto",
    "Geometry": {
        "x": 1,
        "y": 1
    },
    "Image Size in bytes": 320000,
    "Pixels": {
        "x": 400,
        "y": 400
    },
    "Max Frames Per File": 100000,
    "Frame Discard Policy": "nodiscard",
    "Frame Padding": 1,
    "Scan Parameters": "[disabled]",
    "Total Frames": 1,
    "Receiver Roi": {
        "xmin": 4294967295,
        "xmax": 4294967295,
        "ymin": 4294967295,
        "ymax": 4294967295
    },
    "Exptime": "20us",
    "Period": "2ms",
    "Ten Giga": 0,
    "ADC Mask": "0xffffffff",
    "Analog Samples": 5000,
    "Frames in File": 1,
    "Frame Header Format": {
        "Frame Number": "8 bytes",
        "SubFrame Number/ExpLength": "4 bytes",
        "Packet Number": "4 bytes",
        "Bunch ID": "8 bytes",
        "Timestamp": "8 bytes",
        "Module Id": "2 bytes",
        "Row": "2 bytes",
        "Column": "2 bytes",
        "Reserved": "2 bytes",
        "Debug": "4 bytes",
        "Round Robin Number": "2 bytes",
        "Detector Type": "1 byte",
        "Header Version": "1 byte",
        "Packets Caught Mask": "64 bytes"
    }
})";

    std::istringstream iss(master_content);
    RawMasterFile f(iss, "test_master_0.json");

    REQUIRE(f.version() == "7.2");
    REQUIRE(f.detector_type() == DetectorType::Moench03_old);
    REQUIRE(f.timing_mode() == TimingMode::Auto);
    REQUIRE(f.detector_layout() == xy{1, 1});
    REQUIRE(f.image_size_in_bytes() == 320000);
    REQUIRE(f.pixels_x() == 400);
    REQUIRE(f.pixels_y() == 400);
    REQUIRE(f.max_frames_per_file() == 100000);
    REQUIRE(f.frame_discard_policy() == FrameDiscardPolicy::NoDiscard);
    REQUIRE(f.frame_padding() == 1);
    REQUIRE(f.total_frames_expected() == 1);
    REQUIRE(f.exptime() == std::chrono::microseconds(20));
    REQUIRE(f.period() == std::chrono::milliseconds(2));
    REQUIRE(f.analog_samples() == 5000);
    REQUIRE(f.digital_samples() == std::nullopt);
    REQUIRE(f.transceiver_samples() == std::nullopt);
    REQUIRE(f.frames_in_file() == 1);
}

TEST_CASE("Parse Moench 7.2 master (SW 8.0.0) from string stream") {
    std::string master_content = R"({
    "Version": 7.2,
    "Timestamp": "Wed Aug 19 09:54:53 2026",
    "Detector Type": "Moench",
    "Timing Mode": "auto",
    "Geometry": {
        "x": 1,
        "y": 1
    },
    "Image Size in bytes": 320000,
    "Pixels": {
        "x": 400,
        "y": 400
    },
    "Max Frames Per File": 100000,
    "Frame Discard Policy": "discardpartial",
    "Frame Padding": 1,
    "Scan Parameters": "[disabled]",
    "Total Frames": 1,
    "Receiver Roi": {
        "xmin": 4294967295,
        "xmax": 4294967295,
        "ymin": 4294967295,
        "ymax": 4294967295
    },
    "Exptime": "10us",
    "Period": "2ms",
    "Number of UDP Interfaces": 1,
    "Number of rows": 400,
    "Frames in File": 1,
    "Frame Header Format": {
        "Frame Number": "8 bytes",
        "SubFrame Number/ExpLength": "4 bytes",
        "Packet Number": "4 bytes",
        "Bunch ID": "8 bytes",
        "Timestamp": "8 bytes",
        "Module Id": "2 bytes",
        "Row": "2 bytes",
        "Column": "2 bytes",
        "Reserved": "2 bytes",
        "Debug": "4 bytes",
        "Round Robin Number": "2 bytes",
        "Detector Type": "1 byte",
        "Header Version": "1 byte",
        "Packets Caught Mask": "64 bytes"
    }
})";

    std::istringstream iss(master_content);
    RawMasterFile f(iss, "test_master_0.json");

    REQUIRE(f.version() == "7.2");
    REQUIRE(f.detector_type() == DetectorType::Moench03);
    REQUIRE(f.timing_mode() == TimingMode::Auto);
    REQUIRE(f.detector_layout() == xy{1, 1});
    REQUIRE(f.image_size_in_bytes() == 320000);
    REQUIRE(f.pixels_x() == 400);
    REQUIRE(f.pixels_y() == 400);
    REQUIRE(f.max_frames_per_file() == 100000);
    REQUIRE(f.frame_discard_policy() == FrameDiscardPolicy::DiscardPartial);
    REQUIRE(f.frame_padding() == 1);
    REQUIRE(f.total_frames_expected() == 1);
    REQUIRE(f.exptime() == std::chrono::microseconds(10));
    REQUIRE(f.period() == std::chrono::milliseconds(2));
    REQUIRE(f.number_of_rows() == 400);
    REQUIRE(f.frames_in_file() == 1);
    REQUIRE(f.analog_samples() == std::nullopt);
    REQUIRE(f.digital_samples() == std::nullopt);
    REQUIRE(f.transceiver_samples() == std::nullopt);
    REQUIRE(f.udp_interfaces_per_module() == xy{1, 1});
}

TEST_CASE("Parse CTB 7.2 master (SW 8.0.0) from string stream") {
    std::string master_content = R"({
    "Version": 7.2,
    "Timestamp": "Wed Aug 19 09:56:30 2026",
    "Detector Type": "ChipTestBoard",
    "Timing Mode": "auto",
    "Geometry": {
        "x": 1,
        "y": 1
    },
    "Image Size in bytes": 192000,
    "Pixels": {
        "x": 32,
        "y": 1
    },
    "Max Frames Per File": 20000,
    "Frame Discard Policy": "nodiscard",
    "Frame Padding": 1,
    "Scan Parameters": "[disabled]",
    "Total Frames": 1,
    "Receiver Roi": {
        "xmin": 4294967295,
        "xmax": 4294967295,
        "ymin": 4294967295,
        "ymax": 4294967295
    },
    "Exptime": "0ns",
    "Period": "1ms",
    "Ten Giga": 0,
    "ADC Mask": "0xffffffff",
    "Analog Flag": 1,
    "Analog Samples": 3000,
    "Digital Flag": 0,
    "Digital Samples": 2000,
    "Dbit Offset": 0,
    "Dbit Bitset": 0,
    "Transceiver Mask": "0x3",
    "Transceiver Flag": 0,
    "Transceiver Samples": 1,
    "Frames in File": 1,
    "Frame Header Format": {
        "Frame Number": "8 bytes",
        "SubFrame Number/ExpLength": "4 bytes",
        "Packet Number": "4 bytes",
        "Bunch ID": "8 bytes",
        "Timestamp": "8 bytes",
        "Module Id": "2 bytes",
        "Row": "2 bytes",
        "Column": "2 bytes",
        "Reserved": "2 bytes",
        "Debug": "4 bytes",
        "Round Robin Number": "2 bytes",
        "Detector Type": "1 byte",
        "Header Version": "1 byte",
        "Packets Caught Mask": "64 bytes"
    }
})";

    std::istringstream iss(master_content);
    RawMasterFile f(iss, "test_master_0.json");

    REQUIRE(f.version() == "7.2");
    REQUIRE(f.detector_type() == DetectorType::ChipTestBoard);
    REQUIRE(f.timing_mode() == TimingMode::Auto);
    REQUIRE(f.detector_layout() == xy{1, 1});
    REQUIRE(f.image_size_in_bytes() == 192000);
    REQUIRE(f.pixels_x() == 32);
    REQUIRE(f.pixels_y() == 1);
    REQUIRE(f.max_frames_per_file() == 20000);
    REQUIRE(f.frame_discard_policy() == FrameDiscardPolicy::NoDiscard);
    REQUIRE(f.frame_padding() == 1);
    REQUIRE(f.total_frames_expected() == 1);
    REQUIRE(f.exptime() == std::chrono::nanoseconds(0));
    REQUIRE(f.period() == std::chrono::milliseconds(1));
    REQUIRE(f.analog_samples() == 3000);
    REQUIRE(f.digital_samples() == std::nullopt);
    REQUIRE(f.transceiver_samples() == std::nullopt);
    REQUIRE(f.frames_in_file() == 1);
}

TEST_CASE(
    "Parse CTB 7.2 master (SW 8.0.0) with digital samples from string stream") {
    std::string master_content = R"({
    "Version": 7.2,
    "Timestamp": "Wed Aug 19 09:58:22 2026",
    "Detector Type": "ChipTestBoard",
    "Timing Mode": "auto",
    "Geometry": {
        "x": 1,
        "y": 1
    },
    "Image Size in bytes": 16000,
    "Pixels": {
        "x": 64,
        "y": 1
    },
    "Max Frames Per File": 20000,
    "Frame Discard Policy": "nodiscard",
    "Frame Padding": 1,
    "Scan Parameters": "[disabled]",
    "Total Frames": 1,
    "Receiver Roi": {
        "xmin": 4294967295,
        "xmax": 4294967295,
        "ymin": 4294967295,
        "ymax": 4294967295
    },
    "Exptime": "0ns",
    "Period": "1ms",
    "Ten Giga": 0,
    "ADC Mask": "0xffffffff",
    "Analog Flag": 0,
    "Analog Samples": 3000,
    "Digital Flag": 1,
    "Digital Samples": 2000,
    "Dbit Offset": 0,
    "Dbit Bitset": 0,
    "Transceiver Mask": "0x3",
    "Transceiver Flag": 0,
    "Transceiver Samples": 1,
    "Frames in File": 1,
    "Frame Header Format": {
        "Frame Number": "8 bytes",
        "SubFrame Number/ExpLength": "4 bytes",
        "Packet Number": "4 bytes",
        "Bunch ID": "8 bytes",
        "Timestamp": "8 bytes",
        "Module Id": "2 bytes",
        "Row": "2 bytes",
        "Column": "2 bytes",
        "Reserved": "2 bytes",
        "Debug": "4 bytes",
        "Round Robin Number": "2 bytes",
        "Detector Type": "1 byte",
        "Header Version": "1 byte",
        "Packets Caught Mask": "64 bytes"
    }
})";

    std::istringstream iss(master_content);
    RawMasterFile f(iss, "test_master_0.json");

    REQUIRE(f.version() == "7.2");
    REQUIRE(f.detector_type() == DetectorType::ChipTestBoard);
    REQUIRE(f.timing_mode() == TimingMode::Auto);
    REQUIRE(f.detector_layout() == xy{1, 1});
    REQUIRE(f.image_size_in_bytes() == 16000);
    REQUIRE(f.pixels_x() == 64);
    REQUIRE(f.pixels_y() == 1);
    REQUIRE(f.max_frames_per_file() == 20000);
    REQUIRE(f.frame_discard_policy() == FrameDiscardPolicy::NoDiscard);
    REQUIRE(f.frame_padding() == 1);
    REQUIRE(f.total_frames_expected() == 1);
    REQUIRE(f.exptime() == std::chrono::nanoseconds(0));
    REQUIRE(f.period() == std::chrono::milliseconds(1));
    REQUIRE(f.analog_samples() == std::nullopt);
    REQUIRE(f.digital_samples() == 2000);
    REQUIRE(f.transceiver_samples() == std::nullopt);
    REQUIRE(f.frames_in_file() == 1);
}

TEST_CASE("Parse a CTB file from stream") {
    std::string master_content = R"({
    "Version": 8.0,
    "Timestamp": "Mon Dec 15 10:57:27 2025",
    "Detector Type": "ChipTestBoard",
    "Timing Mode": "auto",
    "Geometry": {
        "x": 1,
        "y": 1
    },
    "Image Size": 18432,
    "Pixels": {
        "x": 2,
        "y": 1
    },
    "Max Frames Per File": 20000,
    "Frame Discard Policy": "nodiscard",
    "Frame Padding": 1,
    "Scan Parameters": {
        "enable": 0,
        "dacInd": 0,
        "start offset": 0,
        "stop offset": 0,
        "step size": 0,
        "dac settle time ns": 0
    },
    "Total Frames": 1,
    "Exposure Time": "0.25s",
    "Acquisition Period": "10ms",
    "Ten Giga": 1,
    "ADC Mask": 4294967295,
    "Analog Flag": 0,
    "Analog Samples": 1,
    "Digital Flag": 0,
    "Digital Samples": 1,
    "Dbit Offset": 0,
    "Dbit Reorder": 1,
    "Dbit Bitset": 0,
    "Transceiver Mask": 3,
    "Transceiver Flag": 1,
    "Transceiver Samples": 1152,
    "Frames in File": 40,
    "Additional JSON Header": {}
})";

    std::istringstream iss(master_content);
    RawMasterFile f(iss, "test_master_0.json");

    REQUIRE(f.version() == "8.0");
    REQUIRE(f.detector_type() == DetectorType::ChipTestBoard);
    REQUIRE(f.timing_mode() == TimingMode::Auto);
    REQUIRE(f.detector_layout().col == 1);
    REQUIRE(f.detector_layout().row == 1);
    REQUIRE(f.image_size_in_bytes() == 18432);
    REQUIRE(f.pixels_x() == 2);
    REQUIRE(f.pixels_y() == 1);
    REQUIRE(f.max_frames_per_file() == 20000);
    // CTB does not have bitdepth in master file, but for the moment we write 16
    // TODO! refactor using std::optional
    // REQUIRE(f.bitdepth() == std::nullopt);
    REQUIRE(f.n_modules() == 1);
    REQUIRE(f.quad() == 0);
    REQUIRE(f.frame_discard_policy() == FrameDiscardPolicy::NoDiscard);
    REQUIRE(f.frame_padding() == 1);
    REQUIRE(f.total_frames_expected() ==
            1); // This is Total Frames in the master file
    REQUIRE(f.exptime() == std::chrono::milliseconds(250));
    REQUIRE(f.period() == std::chrono::milliseconds(10));
    REQUIRE(f.analog_samples() == std::nullopt);  // Analog Flag is 0
    REQUIRE(f.digital_samples() == std::nullopt); // Digital Flag is 0
    REQUIRE(f.transceiver_samples() == 1152);
    REQUIRE(f.frames_in_file() == 40);
    REQUIRE(f.get_reading_mode() == ReadoutMode::TRANSCEIVER_ONLY);
}

TEST_CASE("Parse v8.0 MYTHEN3 from stream") {
    std::string master_content = R"({
    "Version": 8.0,
    "Timestamp": "Wed Oct  1 14:37:26 2025",
    "Detector Type": "Mythen3",
    "Timing Mode": "auto",
    "Geometry": {
        "x": 2,
        "y": 1
    },
    "Image Size": 5120,
    "Pixels": {
        "x": 1280,
        "y": 1
    },
    "Max Frames Per File": 10000,
    "Frame Discard Policy": "nodiscard",
    "Frame Padding": 1,
    "Scan Parameters": {
        "enable": 0,
        "dacInd": 0,
        "start offset": 0,
        "stop offset": 0,
        "step size": 0,
        "dac settle time ns": 0
    },
    "Total Frames": 1,
    "Receiver Rois": [
        {
            "xmin": 0,
            "xmax": 2559,
            "ymin": -1,
            "ymax": -1
        }
    ],
    "Dynamic Range": 32,
    "Ten Giga": 1,
    "Acquisition Period": "0ns",
    "Counter Mask": 4,
    "Exposure Times": [
        "5s",
        "5s",
        "5s"
    ],
    "Gate Delays": [
        "0ns",
        "0ns",
        "0ns"
    ],
    "Gates": 1,
    "Threshold Energies": [
        -1,
        -1,
        -1
    ],
    "Readout Speed": "half_speed",
    "Frames in File": 1,
    "Additional JSON Header": {}
})";

    std::istringstream iss(master_content);
    RawMasterFile f(iss, "test_master_0.json");

    REQUIRE(f.version() == "8.0");
    REQUIRE(f.detector_type() == DetectorType::Mythen3);
    REQUIRE(f.timing_mode() == TimingMode::Auto);
    REQUIRE(f.detector_layout().col == 2);
    REQUIRE(f.detector_layout().row == 1);
    REQUIRE(f.image_size_in_bytes() == 5120);
    REQUIRE(f.pixels_x() == 1280);
    REQUIRE(f.pixels_y() == 1);
    REQUIRE(f.max_frames_per_file() == 10000);
    REQUIRE(f.n_modules() == 2);
    REQUIRE(f.quad() == 0);
    REQUIRE(f.frame_discard_policy() == FrameDiscardPolicy::NoDiscard);
    REQUIRE(f.frame_padding() == 1);
    REQUIRE(f.total_frames_expected() ==
            1); // This is Total Frames in the master file
    REQUIRE(f.counter_mask() == 4);
    REQUIRE(f.bitdepth() == 32);

    // Mythen3 has three exposure times, but for the moment we don't handle them
    REQUIRE(f.exptime() == std::nullopt);

    // Period is ok though
    REQUIRE(f.period() == std::chrono::nanoseconds(0));
}

TEST_CASE("Parse a v7.1 Mythen3 from stream") {
    std::string master_content = R"({
    "Version": 7.1,
    "Timestamp": "Wed Sep 21 13:48:10 2022",
    "Detector Type": "Mythen3",
    "Timing Mode": "auto",
    "Geometry": {
        "x": 1,
        "y": 1
    },
    "Image Size in bytes": 15360,
    "Pixels": {
        "x": 3840,
        "y": 1
    },
    "Max Frames Per File": 10000,
    "Frame Discard Policy": "nodiscard",
    "Frame Padding": 1,
    "Scan Parameters": "[disabled]",
    "Total Frames": 1,
    "Receiver Roi": {
        "xmin": 4294967295,
        "xmax": 4294967295,
        "ymin": 4294967295,
        "ymax": 4294967295
    },
    "Dynamic Range": 32,
    "Ten Giga": 1,
    "Period": "2ms",
    "Counter Mask": "0x7",
    "Exptime1": "0.1s",
    "Exptime2": "0.1s",
    "Exptime3": "0.1s",
    "GateDelay1": "0ns",
    "GateDelay2": "0ns",
    "GateDelay3": "0ns",
    "Gates": 1,
    "Threshold Energies": "[-1, -1, -1]",
    "Frames in File": 1,
    "Frame Header Format": {
        "Frame Number": "8 bytes",
        "SubFrame Number/ExpLength": "4 bytes",
        "Packet Number": "4 bytes",
        "Bunch ID": "8 bytes",
        "Timestamp": "8 bytes",
        "Module Id": "2 bytes",
        "Row": "2 bytes",
        "Column": "2 bytes",
        "Reserved": "2 bytes",
        "Debug": "4 bytes",
        "Round Robin Number": "2 bytes",
        "Detector Type": "1 byte",
        "Header Version": "1 byte",
        "Packets Caught Mask": "64 bytes"
    }
})";

    std::istringstream iss(master_content);
    RawMasterFile f(iss, "test_master_0.json");

    REQUIRE(f.version() == "7.1");
    REQUIRE(f.detector_type() == DetectorType::Mythen3);
    REQUIRE(f.timing_mode() == TimingMode::Auto);
    REQUIRE(f.detector_layout().col == 1);
    REQUIRE(f.detector_layout().row == 1);
    REQUIRE(f.image_size_in_bytes() == 15360);
    REQUIRE(f.pixels_x() == 3840);
    REQUIRE(f.pixels_y() == 1);
    REQUIRE(f.max_frames_per_file() == 10000);
    REQUIRE(f.n_modules() == 1);
    REQUIRE(f.quad() == 0);
    REQUIRE(f.frame_discard_policy() == FrameDiscardPolicy::NoDiscard);
    REQUIRE(f.frame_padding() == 1);
    REQUIRE(f.total_frames_expected() ==
            1); // This is Total Frames in the master file
    REQUIRE(f.counter_mask() == 0x7);
    REQUIRE(f.bitdepth() == 32);

    // Mythen3 has three exposure times, but for the moment we don't handle them
    REQUIRE(f.exptime() == std::nullopt);

    // Period is ok though
    REQUIRE(f.period() == std::chrono::milliseconds(2));
}

TEST_CASE("Parse old Moench03 from stream") {
    std::string master_content = R"(
    {
    "Version": 7.1,
    "Timestamp": "Mon Mar 25 10:07:02 2024",
    "Detector Type": "Moench",
    "Timing Mode": "auto",
    "Geometry": {
        "x": 1,
        "y": 1
    },
    "Image Size in bytes": 320000,
    "Pixels": {
        "x": 400,
        "y": 400
    },
    "Max Frames Per File": 100000,
    "Frame Discard Policy": "discardpartial",
    "Frame Padding": 1,
    "Scan Parameters": "[disabled]",
    "Total Frames": 1000000,
    "Receiver Roi": {
        "xmin": 4294967295,
        "xmax": 4294967295,
        "ymin": 4294967295,
        "ymax": 4294967295
    },
    "Exptime": "50us",
    "Period": "600us",
    "Ten Giga": 1,
    "ADC Mask": "0xffffffff",
    "Analog Samples": 5000,
    "Additional Json Header": "{detectorMode: analog, frameMode: newPedestal}",
    "Frames in File": 999995,
    "Frame Header Format": {
        "Frame Number": "8 bytes",
        "SubFrame Number/ExpLength": "4 bytes",
        "Packet Number": "4 bytes",
        "Bunch ID": "8 bytes",
        "Timestamp": "8 bytes",
        "Module Id": "2 bytes",
        "Row": "2 bytes",
        "Column": "2 bytes",
        "Reserved": "2 bytes",
        "Debug": "4 bytes",
        "Round Robin Number": "2 bytes",
        "Detector Type": "1 byte",
        "Header Version": "1 byte",
        "Packets Caught Mask": "64 bytes"
    }
}

)";

    std::istringstream iss(master_content);
    RawMasterFile f(iss, "test_master_0.json");

    REQUIRE(f.version() == "7.1");
    REQUIRE(f.detector_type() == DetectorType::Moench03_old);
    REQUIRE(f.timing_mode() == TimingMode::Auto);
    REQUIRE(f.detector_layout().col == 1);
    REQUIRE(f.detector_layout().row == 1);
    REQUIRE(f.image_size_in_bytes() == 320000);
    REQUIRE(f.pixels_x() == 400);
    REQUIRE(f.pixels_y() == 400);
    REQUIRE(f.max_frames_per_file() == 100000);
    REQUIRE((f.total_frames_expected() ==
             1000000)); // This is Total Frames in the master file
    REQUIRE(f.frames_in_file() == 999995);
    REQUIRE(f.frame_discard_policy() == FrameDiscardPolicy::DiscardPartial);
    REQUIRE(f.frame_padding() == 1);
    REQUIRE(f.n_modules() == 1);
    REQUIRE(f.quad() == 0);
    REQUIRE(f.bitdepth() == 16);
    REQUIRE(f.exptime() == std::chrono::microseconds(50));
    REQUIRE(f.period() == std::chrono::microseconds(600));
    REQUIRE(f.analog_samples() == 5000);
}

TEST_CASE("Parse Eiger json v8.1 all ports active") {
    std::string master_content = R"(
    {
    "Version": 8.1,
    "Timestamp": "Mon Aug 10 17:44:46 2026",
    "Detector Type": "Eiger",
    "Timing Mode": "auto",
    "Geometry": {
        "x": 2,
        "y": 2
    },
    "Image Size": 262144,
    "Pixels": {
        "x": 512,
        "y": 256
    },
    "Max Frames Per File": 10000,
    "Frame Discard Policy": "nodiscard",
    "Frame Padding": 1,
    "Scan Parameters": {
        "enable": 0,
        "dacInd": 0,
        "start offset": 0,
        "stop offset": 0,
        "step size": 0,
        "dac settle time ns": 0
    },
    "Total Frames": 5,
    "Receiver Rois": [
        {
            "xmin": 0,
            "xmax": 1023,
            "ymin": 0,
            "ymax": 511
        }
    ],
    "Dynamic Range": 16,
    "Ten Giga": 0,
    "Exposure Time": "1s",
    "Acquisition Period": "1s",
    "Threshold Energy": -1,
    "Sub Exposure Time": "2.62144ms",
    "Sub Acquisition Period": "2.62144ms",
    "Quad": 0,
    "UDP Ports Type": [
        "left",
        "right"
    ],
    "UDP Ports Disabled": [],
    "Number of Rows": 256,
    "Rate Corrections": [
        0,
        0
    ],
    "Readout Speed": "full_speed",
    "Frames in File": 5,
    "Additional JSON Header": {}
}

)";

    std::istringstream iss(master_content);
    RawMasterFile f(iss, "test_master_0.json");

    REQUIRE(f.version() == "8.1");
    REQUIRE(f.detector_type() == DetectorType::Eiger);
    REQUIRE(f.timing_mode() == TimingMode::Auto);
    REQUIRE(f.detector_layout() == xy{2, 2});
    REQUIRE(f.n_modules() == 4);
    REQUIRE(f.image_size_in_bytes() == 262144);
    REQUIRE(f.pixels_x() == 512);
    REQUIRE(f.pixels_y() == 256);
    REQUIRE(f.max_frames_per_file() == 10000);
    REQUIRE(f.frame_discard_policy() == FrameDiscardPolicy::NoDiscard);
    REQUIRE(f.frame_padding() == 1);
    REQUIRE(f.total_frames_expected() == 5);
    REQUIRE(f.frames_in_file() == 5);
    REQUIRE(f.bitdepth() == 16);
    REQUIRE(f.exptime() == std::chrono::seconds(1));
    REQUIRE(f.period() == std::chrono::seconds(1));
    REQUIRE(f.quad() == 0);
    //rows return a std::optional, so we need to check if it has a value before using it
    auto rows = f.number_of_rows();
    REQUIRE(rows.has_value());
    REQUIRE(rows.value() == 256);
    REQUIRE(f.udp_interfaces_per_module() == xy{1, 2});

    auto scan_parameters = f.scan_parameters();
    REQUIRE_FALSE(scan_parameters.enabled());
    REQUIRE(scan_parameters.dac() == DACIndex::DAC_0);
    REQUIRE(scan_parameters.start() == 0);
    REQUIRE(scan_parameters.stop() == 0);
    REQUIRE(scan_parameters.step() == 0);
    REQUIRE(scan_parameters.settleTime() == 0);

    REQUIRE(f.udp_port_types().has_value());
    REQUIRE(f.udp_port_types().value() ==
            std::vector<UDPPortPosition>{UDPPortPosition::LEFT,
                                         UDPPortPosition::RIGHT});
    REQUIRE(f.disabled_udp_ports().has_value());
    REQUIRE(f.disabled_udp_ports().value().empty());

    auto rois = f.rois();
    REQUIRE(rois.size() == 1);
    REQUIRE(rois[0] == ROI{0, 1024, 0, 512});
}
