/************************************************
 * @file AngleCalibration.test.cpp
 * @short test case for angle calibration class
 ***********************************************/

#include "aare/AngleCalibration.hpp"

#include <filesystem>

#include "test_config.hpp"

#include <catch2/catch_all.hpp>
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

using namespace aare;

class AngleCalibrationTestClass : public AngleCalibration {

  public:
    AngleCalibrationTestClass(
        std::shared_ptr<MythenDetectorSpecifications> mythen_detector_,
        std::shared_ptr<FlatField> flat_field_,
        std::shared_ptr<MythenFileReader> mythen_file_reader_)
        : AngleCalibration(mythen_detector_, flat_field_, mythen_file_reader_) {
    }
    ~AngleCalibrationTestClass() = default;

    std::vector<double> get_centers() { return centers; }

    std::vector<double> get_conversions() { return conversions; }

    std::vector<double> get_offsets() { return offsets; }
};

TEST_CASE("read initial angle calibration file",
          "[.anglecalibration] [.files]") {

    auto fpath = test_data_path() / "AngleCalibration_Test_Data";

    REQUIRE(std::filesystem::exists(fpath));

    std::shared_ptr<MythenDetectorSpecifications> mythen_detector_ptr =
        std::make_shared<MythenDetectorSpecifications>();

    std::shared_ptr<FlatField> flat_field_ptr =
        std::make_shared<FlatField>(mythen_detector_ptr);

    std::shared_ptr<MythenFileReader> mythen_file_reader_ptr =
        std::make_shared<MythenFileReader>(fpath,
                                           "ang1up_22keV_LaB60p3mm_48M_a_0");

    AngleCalibrationTestClass anglecalibration(
        mythen_detector_ptr, flat_field_ptr, mythen_file_reader_ptr);

    std::string filename = fpath / "Angcal_2E_Feb2023_P29.off";

    REQUIRE(std::filesystem::exists(filename));

    anglecalibration.read_initial_calibration_from_file(filename);

    auto centers = anglecalibration.get_centers();
    auto conversions = anglecalibration.get_conversions();
    auto offsets = anglecalibration.get_offsets();

    std::cout.precision(17);

    CHECK(centers.size() == 48);
    CHECK(conversions.size() == 48);
    CHECK(offsets.size() == 48);

    CHECK(centers[9] == Catch::Approx(660.342326));
    CHECK(offsets[47] == Catch::Approx(5.8053312));
    CHECK(conversions[27] == Catch::Approx(-0.6581179125e-4));
}

TEST_CASE("read bad channels",
          "[.anglecalibration][.mythenspecifications][.files]") {
    auto fpath = test_data_path() / "AngleCalibration_Test_Data";

    REQUIRE(std::filesystem::exists(fpath));

    MythenDetectorSpecifications mythen_detector;

    std::string bad_channels_filename = fpath / "bc2023_003_RING.chans";

    REQUIRE(std::filesystem::exists(bad_channels_filename));

    mythen_detector.read_bad_channels_from_file(bad_channels_filename);

    CHECK(mythen_detector.get_bad_channels().size() == 61440);

    CHECK(mythen_detector.get_bad_channels()[61437] == true);
    CHECK(std::all_of(mythen_detector.get_bad_channels().begin() + 30720,
                      mythen_detector.get_bad_channels().begin() + 61439,
                      [](const bool element) { return element; }));
}

TEST_CASE("read unconnected modules",
          "[.anglecalibration][.mythenspecifications][.files]") {
    auto fpath = test_data_path() / "AngleCalibration_Test_Data";

    REQUIRE(std::filesystem::exists(fpath));

    MythenDetectorSpecifications mythen_detector;

    std::string unconnected_modules_filename = fpath / "ModOut.txt";

    REQUIRE(std::filesystem::exists(unconnected_modules_filename));

    mythen_detector.read_unconnected_modules_from_file(
        unconnected_modules_filename);

    CHECK(mythen_detector.get_connected_modules().size() == 48);

    CHECK(std::all_of(mythen_detector.get_connected_modules().begin(),
                      mythen_detector.get_connected_modules().end(),
                      [](const bool element) { return element; }));
}

TEST_CASE("read flatfield", "[.anglecalibration][.flatfield][.files]") {
    auto fpath = test_data_path() / "AngleCalibration_Test_Data";

    REQUIRE(std::filesystem::exists(fpath));

    std::shared_ptr<MythenDetectorSpecifications> mythen_detector_ptr =
        std::make_shared<MythenDetectorSpecifications>();

    FlatField flatfield(mythen_detector_ptr);

    std::string flatfield_filename =
        fpath /
        "Flatfield_E22p0keV_T11000eV_up_48M_a_LONG_Feb2023_open_WS_SUMC.raw";

    REQUIRE(std::filesystem::exists(flatfield_filename));

    flatfield.read_flatfield_from_file(flatfield_filename);

    auto flatfield_data = flatfield.get_flatfield();

    CHECK(flatfield_data.size() == 61440);

    CHECK(flatfield_data[0] == 0);
    CHECK(flatfield_data[21] == 4234186);
}

TEST_CASE("calculate new fixed angle width bins histogram",
          "[.anglecalibration] [.files]") {

    auto fpath = test_data_path() / "AngleCalibration_Test_Data";

    REQUIRE(std::filesystem::exists(fpath));

    std::shared_ptr<MythenDetectorSpecifications> mythen_detector_ptr =
        std::make_shared<MythenDetectorSpecifications>();

    std::string bad_channels_filename = fpath / "bc2023_003_RING.chans";

    REQUIRE(std::filesystem::exists(bad_channels_filename));

    mythen_detector_ptr->read_bad_channels_from_file(bad_channels_filename);

    std::string unconnected_modules_filename = fpath / "ModOut.txt";

    REQUIRE(std::filesystem::exists(unconnected_modules_filename));

    mythen_detector_ptr->read_unconnected_modules_from_file(
        unconnected_modules_filename);

    std::shared_ptr<FlatField> flat_field_ptr =
        std::make_shared<FlatField>(mythen_detector_ptr);

    std::string flatfield_filename =
        fpath /
        "Flatfield_E22p0keV_T11000eV_up_48M_a_LONG_Feb2023_open_WS_SUMC.raw";

    REQUIRE(std::filesystem::exists(flatfield_filename));

    flat_field_ptr->read_flatfield_from_file(flatfield_filename);

    std::shared_ptr<MythenFileReader> mythen_file_reader_ptr =
        std::make_shared<MythenFileReader>(fpath,
                                           "ang1up_22keV_LaB60p3mm_48M_a_0");

    AngleCalibration anglecalibration(mythen_detector_ptr, flat_field_ptr,
                                      mythen_file_reader_ptr);

    std::string initial_angles_filename = fpath / "Angcal_2E_Feb2023_P29.off";

    REQUIRE(std::filesystem::exists(initial_angles_filename));

    anglecalibration.read_initial_calibration_from_file(
        initial_angles_filename);

    anglecalibration.calculate_fixed_bin_angle_width_histogram(320, 340);

    anglecalibration.write_to_file(
        "cpp_new_photon_counts.xye"); // TODO adjust output path
}
