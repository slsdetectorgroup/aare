/************************************************
 * @file AngleCalibration.test.cpp
 * @short test case for angle calibration class
 ***********************************************/

#include "aare/AngleCalibration.hpp"

#include <filesystem>

#include "test_config.hpp"

#include <iomanip>
#include <type_traits>

#include <catch2/catch_all.hpp>
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

using namespace aare;

template <typename T, ssize_t Ndim = 1>
NDArray<T, Ndim> read_into_array(const std::string &filename,
                                 const std::array<ssize_t, Ndim> size) {
    std::string word;
    NDArray<T, Ndim> array(size);
    try {
        std::ifstream file(filename, std::ios_base::in);
        if (!file.good()) {
            throw std::logic_error("file does not exist");
        }

        std::stringstream file_buffer;
        file_buffer << file.rdbuf();

        ssize_t counter = 0;
        while (file_buffer >> word) {
            array[counter] = std::stod(word); // TODO change for different Types
            ++counter;
        }

        file.close();
    } catch (const std::exception &e) {
        std::cerr << "Error: " << e.what() << std::endl;
    }

    return array;
}

template <typename T, bool = std::is_integral_v<T>> struct safe_make_signed {
    using type = T;
};

template <typename T> struct safe_make_signed<T, true> {
    using type = std::make_signed_t<T>;
};

template <typename T, ssize_t Ndim>
bool check_equality_of_arrays(NDView<T, Ndim> array1, NDView<T, Ndim> array2) {
    bool equal = true;

    using SignedT = typename safe_make_signed<T>::type;

    for (ssize_t i = 0; i < array1.size(); ++i) {
        if (std::abs(static_cast<SignedT>(array1[i]) -
                     static_cast<SignedT>(array2[i])) > 1e-6) {
            std::cout << "index: " << i << std::endl;
            std::cout << std::setprecision(15) << array1[i] << std::endl;
            std::cout << std::setprecision(15) << array2[i] << std::endl;
            equal = false;
            break;
        }
    }

    return equal;
}

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

TEST_CASE("check flatfield values", "[.anglecalibration][.flatfield][.files]") {
    auto fpath = test_data_path() / "AngleCalibration_Test_Data";

    REQUIRE(std::filesystem::exists(fpath));

    std::shared_ptr<MythenDetectorSpecifications> mythen_detector_ptr =
        std::make_shared<MythenDetectorSpecifications>();

    std::string bad_channels_filename = fpath / "bc2023_003_RING.chans";

    REQUIRE(std::filesystem::exists(bad_channels_filename));

    mythen_detector_ptr->read_bad_channels_from_file(bad_channels_filename);

    FlatField flatfield(mythen_detector_ptr);

    std::string flatfield_filename =
        fpath /
        "Flatfield_E22p0keV_T11000eV_up_48M_a_LONG_Feb2023_open_WS_SUMC.raw";

    REQUIRE(std::filesystem::exists(flatfield_filename));

    flatfield.read_flatfield_from_file(flatfield_filename);

    auto flatfield_data = flatfield.get_flatfield();

    std::string expected_flatfield_filename = fpath / "flatfield.txt";

    REQUIRE(std::filesystem::exists(expected_flatfield_filename));

    NDArray<double, 1> expected_flatfield = read_into_array<double, 1>(
        expected_flatfield_filename, flatfield_data.shape());

    auto bad_channels = mythen_detector_ptr->get_bad_channels();

    bool equal_flatfield = true;
    for (ssize_t i = 0; i < flatfield_data.size(); ++i) {
        if (!bad_channels[i] &&
            std::abs(flatfield_data[i] - expected_flatfield[i]) >
                std::numeric_limits<double>::epsilon()) {
            std::cout << "index: " << i << std::endl;
            std::cout << flatfield_data[i] << std::endl;
            std::cout << expected_flatfield[i] << std::endl;
            equal_flatfield = false;
            break;
        }
    }
    CHECK(equal_flatfield);
}

TEST_CASE("check inverse flatfield values",
          "[.anglecalibration][.flatfield][.files]") {
    auto fpath = test_data_path() / "AngleCalibration_Test_Data";

    REQUIRE(std::filesystem::exists(fpath));

    std::shared_ptr<MythenDetectorSpecifications> mythen_detector_ptr =
        std::make_shared<MythenDetectorSpecifications>();

    std::string bad_channels_filename = fpath / "bc2023_003_RING.chans";

    REQUIRE(std::filesystem::exists(bad_channels_filename));

    mythen_detector_ptr->read_bad_channels_from_file(bad_channels_filename);

    FlatField flatfield(mythen_detector_ptr);

    std::string flatfield_filename =
        fpath /
        "Flatfield_E22p0keV_T11000eV_up_48M_a_LONG_Feb2023_open_WS_SUMC.raw";

    REQUIRE(std::filesystem::exists(flatfield_filename));

    flatfield.read_flatfield_from_file(flatfield_filename);

    auto inverse_flatfield = flatfield.inverse_normalized_flatfield();

    std::string expected_inverseflatfield_filename =
        fpath / "inverseflatfield.txt";

    REQUIRE(std::filesystem::exists(expected_inverseflatfield_filename));

    NDArray<double, 1> expected_inverseflatfield = read_into_array<double, 1>(
        expected_inverseflatfield_filename, inverse_flatfield.shape());

    auto bad_channels = mythen_detector_ptr->get_bad_channels();

    bool equal = true;
    for (ssize_t i = 0; i < inverse_flatfield.size(); ++i) {
        if (!bad_channels[i] &&
            std::abs(inverse_flatfield[i] - expected_inverseflatfield[i]) >
                1e-10) {
            std::cout << "index: " << i << std::endl;
            std::cout << std::setprecision(15) << inverse_flatfield[i]
                      << std::endl;
            std::cout << std::setprecision(15) << expected_inverseflatfield[i]
                      << std::endl;
            equal = false;
            break;
        }
    }
    CHECK(equal);
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

TEST_CASE("compare result with python code", "[.anglecalibration][.files]") {
    auto expected_filename =
        test_data_path() / "AngleCalibration_Test_Data" / "out2.xye";

    REQUIRE(std::filesystem::exists(expected_filename));

    auto expected_array = read_into_array<double, 2>(
        expected_filename.string(), std::array<ssize_t, 2>{61440, 3});

    auto filename =
        std::filesystem::current_path() / "../build/cpp_new_photon_counts.xye";

    // auto array = load<double, 2>(filename, std::array<ssize_t, 2>{61440, 3});
    auto array = read_into_array<double, 2>(filename.string(),
                                            std::array<ssize_t, 2>{61440, 3});

    CHECK(check_equality_of_arrays(array.view(), expected_array.view()));
}

TEST_CASE("check conversion from DG to EE parameters",
          "[.anglecalibration][.files]") {

    auto fpath = test_data_path() / "AngleCalibration_Test_Data";

    REQUIRE(std::filesystem::exists(fpath));

    std::shared_ptr<MythenDetectorSpecifications> mythen_detector_ptr =
        std::make_shared<MythenDetectorSpecifications>();

    std::shared_ptr<FlatField> flat_field_ptr =
        std::make_shared<FlatField>(mythen_detector_ptr);

    std::shared_ptr<MythenFileReader> mythen_file_reader_ptr =
        std::make_shared<MythenFileReader>(fpath,
                                           "ang1up_22keV_LaB60p3mm_48M_a_0");

    AngleCalibration anglecalibration(mythen_detector_ptr, flat_field_ptr,
                                      mythen_file_reader_ptr);

    // DG parameters
    double center = 642.197591224993;
    double conversion = 0.657694036246975e-4;
    double offset = 5.004892881251670;

    double global_strip_index =
        MythenDetectorSpecifications::strips_per_module() + 1;

    double diffraction_angle_DG_param =
        anglecalibration.diffraction_angle_from_DG_parameters(
            center, conversion, offset, 1);

    auto [distance_center, normal_distance, angle] =
        anglecalibration.convert_to_EE_parameters(center, conversion, offset);

    double diffraction_angle_EE_param =
        anglecalibration.diffraction_angle_from_EE_parameters(
            distance_center, normal_distance, angle, 1);

    CHECK(diffraction_angle_EE_param ==
          Catch::Approx(diffraction_angle_DG_param));

    double strip_width_DG_param =
        anglecalibration.angular_strip_width_from_DG_parameters(
            center, conversion, offset, global_strip_index);

    double strip_width_EE_param =
        anglecalibration.angular_strip_width_from_EE_parameters(
            distance_center, normal_distance, angle, global_strip_index);

    CHECK(strip_width_DG_param == Catch::Approx(strip_width_EE_param));
}
