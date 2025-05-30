/************************************************
 * @file AngleCalibration.test.cpp
 * @short test case for angle calibration class
 ***********************************************/

#include "aare/AngleCalibration.hpp"

#include <filesystem>

#include <catch2/catch_all.hpp>
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

using namespace aare;

class AngleCalibrationTestClass : public aare::AngleCalibration {

  public:
    AngleCalibrationTestClass(
        std::shared_ptr<MythenDetectorSpecifications> mythen_detector_,
        std::shared_ptr<FlatField> flat_field_,
        std::shared_ptr<MythenFileReader> mythen_file_reader_)
        : aare::AngleCalibration(mythen_detector_, flat_field_,
                                 mythen_file_reader_) {}
    ~AngleCalibrationTestClass() = default;

    std::vector<double> get_centers() { return centers; }

    std::vector<double> get_conversions() { return conversions; }

    std::vector<double> get_offsets() { return offsets; }
};

TEST_CASE("read initial angle calibration file",
          "[.anglecalibration][.fileread]") {

    std::shared_ptr<MythenDetectorSpecifications> mythen_detector_ptr =
        std::make_shared<MythenDetectorSpecifications>();

    std::shared_ptr<FlatField> flat_field_ptr =
        std::make_shared<FlatField>(mythen_detector_ptr);

    std::shared_ptr<MythenFileReader> mythen_file_reader_ptr =
        std::make_shared<MythenFileReader>(
            std::filesystem::path{
                "/home/mazzola/Documents/mythen3tools/beamline/TDATA"},
            "ang1up_22keV_LaB60p3mm_48M_a_0");

    AngleCalibrationTestClass anglecalibration(
        mythen_detector_ptr, flat_field_ptr, mythen_file_reader_ptr);

    std::string filename =
        "/home/mazzol_a/Documents/mythen3tools/beamline/"
        "TDATA/Angcal_2E_Feb2023_P29.off"; // TODO change path upload data

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
