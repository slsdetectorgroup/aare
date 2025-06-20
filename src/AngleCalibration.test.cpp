/************************************************
 * @file AngleCalibration.test.cpp
 * @short test case for angle calibration class
 ***********************************************/

#include "aare/AngleCalibration.hpp"
#include "aare/CustomFiles.hpp"
#include "aare/FlatField.hpp"

#include <filesystem>

#include "test_config.hpp"

#include <iomanip>
#include <type_traits>

#include <catch2/catch_all.hpp>
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

using namespace aare;

TEST_CASE("read initial angle calibration file",
          "[anglecalibration] [.files]") {

    std::shared_ptr<MythenDetectorSpecifications> mythen_detector_ptr =
        std::make_shared<MythenDetectorSpecifications>();

    AngleCalibration anglecalibration(mythen_detector_ptr,
                                      std::shared_ptr<FlatField>{},
                                      std::shared_ptr<MythenFileReader>{});

    std::string filename = test_data_path() / "AngleCalibration_Test_Data" /
                           "Angcal_2E_Feb2023_P29.off";

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
          "[anglecalibration][mythenspecifications][.files]") {

    MythenDetectorSpecifications mythen_detector;

    std::string bad_channels_filename = test_data_path() /
                                        "AngleCalibration_Test_Data" /
                                        "bc2023_003_RING.chans";

    REQUIRE(std::filesystem::exists(bad_channels_filename));

    mythen_detector.read_bad_channels_from_file(bad_channels_filename);

    CHECK(mythen_detector.get_bad_channels().size() == 61440);

    CHECK(mythen_detector.get_bad_channels()[61437] == true);
    CHECK(std::all_of(mythen_detector.get_bad_channels().begin() + 30720,
                      mythen_detector.get_bad_channels().begin() + 61439,
                      [](const bool element) { return element; }));
}

TEST_CASE("read unconnected modules",
          "[anglecalibration][mythenspecifications][.files]") {

    MythenDetectorSpecifications mythen_detector;

    std::string unconnected_modules_filename =
        test_data_path() / "AngleCalibration_Test_Data" / "ModOut.txt";

    REQUIRE(std::filesystem::exists(unconnected_modules_filename));

    mythen_detector.read_unconnected_modules_from_file(
        unconnected_modules_filename);

    CHECK(mythen_detector.get_connected_modules().size() == 48);

    CHECK(std::all_of(mythen_detector.get_connected_modules().begin(),
                      mythen_detector.get_connected_modules().end(),
                      [](const bool element) { return element; }));
}

TEST_CASE("read flatfield", "[anglecalibration][flatfield][.files]") {

    std::shared_ptr<MythenDetectorSpecifications> mythen_detector_ptr =
        std::make_shared<MythenDetectorSpecifications>();

    FlatField flatfield(mythen_detector_ptr);

    std::string flatfield_filename =
        test_data_path() / "AngleCalibration_Test_Data" /
        "Flatfield_E22p0keV_T11000eV_up_48M_a_LONG_Feb2023_open_WS_SUMC.raw";

    REQUIRE(std::filesystem::exists(flatfield_filename));

    flatfield.read_flatfield_from_file<CustomMythenFile>(flatfield_filename);

    auto flatfield_data = flatfield.get_flatfield();

    CHECK(flatfield_data.size() == 61440);

    CHECK(flatfield_data[0] == 0);
    CHECK(flatfield_data[21] == 4234186);
}

TEST_CASE("create flatfield", "[anglecalibration], [flatfield], [.files]") {

    ssize_t n_modules = 2;
    double exposure_time = 5.0;
    size_t n_counters = 1;
    std::shared_ptr<MythenDetectorSpecifications> mythen_detector_ptr =
        std::make_shared<MythenDetectorSpecifications>(n_modules, exposure_time,
                                                       n_counters);

    FlatField flatfield(mythen_detector_ptr);

    auto data_path = test_data_path() / "AngleCalibration_Test_Data" /
                     "Flatfieldacquisition";

    REQUIRE(std::filesystem::exists(data_path));

    CHECK_NOTHROW(flatfield.create_flatfield_from_rawfilesystem(data_path));

    auto flatfield_data = flatfield.get_flatfield();

    CHECK(flatfield_data.size() == n_modules * 1280);

    CHECK(flatfield_data[0] == 0);
    CHECK(flatfield_data[21] ==
          2 * 3 *
              21); // virtual data 2 angles, 3 frames - 21 as increasing numbers
}

TEST_CASE("compare result with python code", "[anglecalibration] [.files]") {

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

    flat_field_ptr->read_flatfield_from_file<CustomMythenFile>(
        flatfield_filename);

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

    // anglecalibration.write_to_file("cpp_new_photon_counts.xye");

    auto expected_filename_photons =
        test_data_path() / "AngleCalibration_Test_Data" / "new_photons.bin";

    REQUIRE(std::filesystem::exists(expected_filename_photons));

    auto expected_filename_errors =
        test_data_path() / "AngleCalibration_Test_Data" / "new_errors.bin";

    REQUIRE(std::filesystem::exists(expected_filename_errors));

    ssize_t new_num_bins = anglecalibration.get_new_num_bins();

    auto python_output_errors = load<double, 1>(
        expected_filename_errors, std::array<ssize_t, 1>{new_num_bins});

    auto python_output_photons = load<double, 1>(
        expected_filename_photons, std::array<ssize_t, 1>{new_num_bins});

    CHECK(anglecalibration.get_new_photon_counts().equals(
        python_output_photons.view(),
        1e-8)); // not sure about precision does not exactly match to all
                // decimal digits

    CHECK(anglecalibration.get_new_statistical_errors().equals(
        python_output_errors.view(),
        1e-8)); //
}

TEST_CASE("check conversion from DG to EE parameters", "[anglecalibration]") {

    std::shared_ptr<MythenDetectorSpecifications> mythen_detector_ptr =
        std::make_shared<MythenDetectorSpecifications>();

    AngleCalibration anglecalibration(mythen_detector_ptr,
                                      std::shared_ptr<FlatField>{},
                                      std::shared_ptr<MythenFileReader>{});

    // DG test parameters
    const double center = 642.197591224993;
    const double conversion = 0.657694036246975e-4;
    const double offset = 5.004892881251670;
    const ssize_t local_strip_index = 1;

    double diffraction_angle_DG_param =
        anglecalibration.diffraction_angle_from_DG_parameters(
            center, conversion, offset, local_strip_index);

    auto [distance_center, normal_distance, angle] =
        anglecalibration.convert_to_EE_parameters(center, conversion, offset);

    double diffraction_angle_EE_param =
        anglecalibration.diffraction_angle_from_EE_parameters(
            distance_center, normal_distance, angle, local_strip_index);

    CHECK(diffraction_angle_EE_param ==
          Catch::Approx(diffraction_angle_DG_param));

    double strip_width_DG_param =
        anglecalibration.angular_strip_width_from_DG_parameters(
            center, conversion, offset, local_strip_index);

    double strip_width_EE_param =
        anglecalibration.angular_strip_width_from_EE_parameters(
            distance_center, normal_distance, angle, local_strip_index);

    CHECK(strip_width_DG_param == Catch::Approx(strip_width_EE_param));
}
