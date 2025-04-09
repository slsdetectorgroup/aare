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

    //Check that the frame number and buch id is read correctly
    for (size_t i = 0; i < 24; ++i) {
        JungfrauDataHeader header;
        aare::NDArray<uint16_t> image(f.shape());
        f.read_into(&image, &header);
        REQUIRE(header.framenum == i + 1);
        REQUIRE(header.bunchid == (i + 1) * (i + 1));
        REQUIRE(image.shape(0) == 512);
        REQUIRE(image.shape(1) == 1024);
    }
}

TEST_CASE("Seek in a JungfrauDataFile", "[.files]"){
    auto fpath = test_data_path() / "dat" / "AldoJF65k_000000.dat";
    REQUIRE(std::filesystem::exists(fpath));

    JungfrauDataFile f(fpath);

    //The file should have 113 frames
    f.seek(19);
    REQUIRE(f.tell() == 19);
    auto h = f.read_header();
    REQUIRE(h.framenum == 19+1);

    //Reading again does not change the file pointer
    auto h2 = f.read_header();
    REQUIRE(h2.framenum == 19+1);

    f.seek(59);
    REQUIRE(f.tell() == 59);
    auto h3 = f.read_header();
    REQUIRE(h3.framenum == 59+1);

    JungfrauDataHeader h4;
    aare::NDArray<uint16_t> image(f.shape());
    f.read_into(&image, &h4);
    REQUIRE(h4.framenum == 59+1);

    //now we should be on the next frame
    REQUIRE(f.tell() == 60);
    REQUIRE(f.read_header().framenum == 60+1);

    REQUIRE_THROWS(f.seek(86356)); //out of range
}

TEST_CASE("Open a Jungfrau data file with non zero file index", "[.files]"){

    auto fpath = test_data_path() / "dat" / "AldoJF65k_000003.dat";
    REQUIRE(std::filesystem::exists(fpath));

    JungfrauDataFile f(fpath);

    //18 files per data file, opening the 3rd file we ignore the first 3
    REQUIRE(f.total_frames() == 113-18*3);
    REQUIRE(f.tell() == 0);

    //Frame numbers start at 1 in the first file
    REQUIRE(f.read_header().framenum == 18*3+1);

    // moving relative to the third file
    f.seek(5);
    REQUIRE(f.read_header().framenum == 18*3+1+5);

    // ignoring the first 3 files
    REQUIRE(f.n_files() == 4);

    REQUIRE(f.current_file().stem() == "AldoJF65k_000003");

}

TEST_CASE("Read into throws if size doesn't match", "[.files]"){
    auto fpath = test_data_path() / "dat" / "AldoJF65k_000000.dat";
    REQUIRE(std::filesystem::exists(fpath));

    JungfrauDataFile f(fpath);

    aare::NDArray<uint16_t> image({39, 85});
    JungfrauDataHeader header;

    REQUIRE_THROWS(f.read_into(&image, &header));
    REQUIRE_THROWS(f.read_into(&image, nullptr));
    REQUIRE_THROWS(f.read_into(&image));

    REQUIRE(f.tell() == 0);


}