#include "aare/JungfrauDataFile.hpp"

#include <catch2/catch_test_macros.hpp>
#include "test_config.hpp"

using aare::JungfrauDataFile;
using aare::JungfrauDataHeader;
TEST_CASE("Open a Jungfrau data file", "[.files]") {
    //we know we have 4 files with 7, 7, 7, and 3 frames
    //firs frame number if 1 and the bunch id is frame_number**2
    //so we can check the header
    auto fpath = test_data_path() / "dat" / "AldoJF500k_000000.dat";
    REQUIRE(std::filesystem::exists(fpath));

    JungfrauDataFile f(fpath);
    REQUIRE(f.rows() == 512);
    REQUIRE(f.cols() == 1024);
    REQUIRE(f.bytes_per_frame() == 1048576);    
    REQUIRE(f.pixels_per_frame() == 524288);
    REQUIRE(f.bytes_per_pixel() == 2);
    REQUIRE(f.bitdepth() == 16);
    REQUIRE(f.base_name() == "AldoJF500k");
    REQUIRE(f.n_files() == 4);
    REQUIRE(f.tell() == 0);
    REQUIRE(f.total_frames() == 24);
    REQUIRE(f.current_file() == fpath);

    for (size_t i = 0; i < 24; ++i) {
        JungfrauDataHeader header;
        auto image = f.read_frame(header);
        REQUIRE(header.framenum == i + 1);
        REQUIRE(header.bunchid == (i + 1) * (i + 1));
        REQUIRE(image.shape(0) == 512);
        REQUIRE(image.shape(1) == 1024);
    }
}