#include "aare/Hdf5MasterFile.hpp"
#include "aare/to_string.hpp"

#include "test_config.hpp"
#include <catch2/catch_test_macros.hpp>

using namespace aare;

TEST_CASE("Parse a master file in .h5 format", "[.integration]") {
    auto fpath = test_data_path() / "hdf5" / "virtual" / "jungfrau" /
                 "two_modules_master_0.h5";
    REQUIRE(std::filesystem::exists(fpath));
    Hdf5MasterFile f(fpath);

    // "Version": 7.2,
    REQUIRE(f.version() == "6.6");
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
    REQUIRE(f.geometry().row == 2);

    // "Image Size in bytes": 1048576,
    REQUIRE(f.image_size_in_bytes() == 1048576);
    // "Pixels": {
    //     "x": 1024,
    REQUIRE(f.pixels_x() == 1024);
    //     "y": 512
    REQUIRE(f.pixels_y() == 512);
    // },

    // "Max Frames Per File": 3,
    REQUIRE(f.max_frames_per_file() == 10000);

    // Jungfrau doesn't write but it is 16
    REQUIRE(f.bitdepth() == 16);

    // "Frame Discard Policy": "nodiscard",
    REQUIRE(f.frame_discard_policy() == FrameDiscardPolicy::NoDiscard);
    // "Frame Padding": 1,
    REQUIRE(f.frame_padding() == 1);
    // "Scan Parameters": "[disabled]",
    REQUIRE(!f.scan_parameters());
    // "Total Frames": 10,
    REQUIRE(f.total_frames_expected() == 5);
    // "Receiver Roi": {
    //     "xmin": 4294967295,
    //     "xmax": 4294967295,
    //     "ymin": 4294967295,
    //     "ymax": 4294967295
    // },
    // "Exptime": "10us",
    REQUIRE(ToString(f.exptime()) == "10us");
    // "Period": "1ms",
    // "Number of UDP Interfaces": 1,
    // "Number of rows": 512,
    REQUIRE(f.number_of_rows() == 512);
    // "Frames in File": 10,
    REQUIRE(f.frames_in_file() == 5);

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
}
