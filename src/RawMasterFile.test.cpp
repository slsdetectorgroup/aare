#include "aare/RawMasterFile.hpp"

#include <catch2/catch_test_macros.hpp>
#include "test_config.hpp"

using namespace aare;


TEST_CASE("Parse a master file fname"){
    RawFileNameComponents m("test_master_1.json");
    REQUIRE(m.base_name() == "test");
    REQUIRE(m.ext() == ".json");
    REQUIRE(m.file_index() == 1);
}

TEST_CASE("Construction of master file name and data files"){
    RawFileNameComponents m("test_master_1.json");
    REQUIRE(m.master_fname() == "test_master_1.json");
    REQUIRE(m.data_fname(0, 0) == "test_d0_f0_1.raw");
    REQUIRE(m.data_fname(1, 0) == "test_d1_f0_1.raw");
    REQUIRE(m.data_fname(0, 1) == "test_d0_f1_1.raw");
    REQUIRE(m.data_fname(1, 1) == "test_d1_f1_1.raw");
}


TEST_CASE("Parse a master file"){
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
    // "Scan Parameters": "[disabled]",
    // "Total Frames": 10,
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
    // "Frames in File": 10,
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


TEST_CASE("Read eiger master file"){
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