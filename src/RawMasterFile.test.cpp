#include "aare/RawMasterFile.hpp"

#include <catch2/catch_test_macros.hpp>
#include "test_config.hpp"

using namespace aare;


TEST_CASE("Parse a master file fname"){
    RawFileNameComponents m("test_master_1.json");
    REQUIRE(m.base_name() == "test");
    REQUIRE(m.ext() == ".json");
    REQUIRE(m.file_index() == 1);
    REQUIRE(m.base_path() == "");
}

TEST_CASE("Extraction of base path works"){
    RawFileNameComponents m("some/path/test_master_73.json");
    REQUIRE(m.base_name() == "test");
    REQUIRE(m.ext() == ".json");
    REQUIRE(m.file_index() == 73);
    REQUIRE(m.base_path() == "some/path");
}

TEST_CASE("Construction of master file name and data files"){
    RawFileNameComponents m("test_master_1.json");
    REQUIRE(m.master_fname() == "test_master_1.json");
    REQUIRE(m.data_fname(0, 0) == "test_d0_f0_1.raw");
    REQUIRE(m.data_fname(1, 0) == "test_d1_f0_1.raw");
    REQUIRE(m.data_fname(0, 1) == "test_d0_f1_1.raw");
    REQUIRE(m.data_fname(1, 1) == "test_d1_f1_1.raw");
}

TEST_CASE("Construction of master file name and data files using old scheme"){
    RawFileNameComponents m("test_master_1.raw");
    m.set_old_scheme(true);
    REQUIRE(m.master_fname() == "test_master_1.raw");
    REQUIRE(m.data_fname(0, 0) == "test_d0_f000000000000_1.raw");
    REQUIRE(m.data_fname(1, 0) == "test_d1_f000000000000_1.raw");
    REQUIRE(m.data_fname(0, 1) == "test_d0_f000000000001_1.raw");
    REQUIRE(m.data_fname(1, 1) == "test_d1_f000000000001_1.raw");
}

TEST_CASE("Master file name does not fit pattern"){
    REQUIRE_THROWS(RawFileNameComponents("somefile.json"));
    REQUIRE_THROWS(RawFileNameComponents("another_test_d0_f0_1.raw"));
    REQUIRE_THROWS(RawFileNameComponents("test_master_1.txt"));
}



TEST_CASE("Parse scan parameters"){
    ScanParameters s("[enabled\ndac dac 4\nstart 500\nstop 2200\nstep 5\nsettleTime 100us\n]");
    REQUIRE(s.enabled());
    REQUIRE(s.dac() == "dac 4");
    REQUIRE(s.start() == 500);
    REQUIRE(s.stop() == 2200);
    REQUIRE(s.step() == 5);
}

TEST_CASE("A disabled scan"){
    ScanParameters s("[disabled]");
    REQUIRE_FALSE(s.enabled());
    REQUIRE(s.dac() == "");
    REQUIRE(s.start() == 0);
    REQUIRE(s.stop() == 0);
    REQUIRE(s.step() == 0);
}


TEST_CASE("Parse a master file in .json format", "[.integration]"){
    auto fpath = test_data_path() / "jungfrau" / "jungfrau_single_master_0.json";
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
    REQUIRE(f.geometry().col == 1);
    REQUIRE(f.geometry().row == 1);

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

    //Jungfrau doesn't write but it is 16
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

    //TODO! Should we parse this? 
    // "Frame Header Format": {
    //     "Frame Number": "8 bytes",
    //     "SubFrame Number/ExpLength": "4 bytes",
    //     "Packet Number": "4 bytes",
    //     "Bunch ID": "8 bytes",
    //     "Timestamp": "8 bytes",
    //     "Module Id": "2 bytes",
    //     "Row": "2 bytes",
    //     "Column": "2 bytes",
    //     "Reserved": "2 bytes",
    //     "Debug": "4 bytes",
    //     "Round Robin Number": "2 bytes",
    //     "Detector Type": "1 byte",
    //     "Header Version": "1 byte",
    //     "Packets Caught Mask": "64 bytes"
    // }
    // }         

    REQUIRE_FALSE(f.analog_samples());
    REQUIRE_FALSE(f.digital_samples());                

}

TEST_CASE("Parse a master file in .raw format", "[.integration]"){
    
    auto fpath = test_data_path() / "moench/moench04_noise_200V_sto_both_100us_no_light_thresh_900_master_0.raw";
    REQUIRE(std::filesystem::exists(fpath));
    RawMasterFile f(fpath);

    // Version                    : 6.4
    REQUIRE(f.version() == "6.4");
    // TimeStamp                  : Wed Aug 31 09:08:49 2022

    // Detector Type              : ChipTestBoard
    REQUIRE(f.detector_type() == DetectorType::ChipTestBoard);
    // Timing Mode                : auto
    REQUIRE(f.timing_mode() == TimingMode::Auto);
    // Geometry                   : [1, 1]
    REQUIRE(f.geometry().col == 1);
    REQUIRE(f.geometry().row == 1);
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
    // Period                     : 4ms
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


TEST_CASE("Read eiger master file", "[.integration]"){
auto fpath = test_data_path() / "eiger" / "eiger_500k_32bit_master_0.json";
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
//     "Exptime": "5s",
//     "Period": "1s",
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