#include "aare/Hdf5MasterFile.hpp"
#include "aare/to_string.hpp"

#include "test_config.hpp"
#include <catch2/catch_test_macros.hpp>

using namespace aare;

TEST_CASE("Parse a multi module jungfrau master file in .h5 format", "[.integration][.hdf5]") {
    auto fpath = test_data_path() / "hdf5" / "virtual" / "jungfrau" /
                 "two_modules_master_0.h5";
    REQUIRE(std::filesystem::exists(fpath));
    Hdf5MasterFile f(fpath);

    REQUIRE(f.version() == "6.6");
    // "Timestamp": "Tue Feb 20 08:28:24 2024",
    REQUIRE(f.detector_type() == DetectorType::Jungfrau);
    REQUIRE(f.timing_mode() == TimingMode::Auto);
    REQUIRE(f.geometry().col == 1);
    REQUIRE(f.geometry().row == 2);
    REQUIRE(f.image_size_in_bytes() == 1048576);
    REQUIRE(f.pixels_x() == 1024);
    REQUIRE(f.pixels_y() == 512);
    REQUIRE(f.max_frames_per_file() == 10000);
    REQUIRE(f.frame_discard_policy() == FrameDiscardPolicy::NoDiscard);
    REQUIRE(f.frame_padding() == 1);
    REQUIRE(f.scan_parameters()->enabled() == false);
    REQUIRE(f.total_frames_expected() == 5);
    REQUIRE(f.exptime() == std::chrono::microseconds(10));
    REQUIRE(f.period() == std::chrono::milliseconds(2));
    REQUIRE_FALSE(f.burst_mode().has_value()); 
    REQUIRE(f.number_of_udp_interfaces() == 1);
    // Jungfrau doesn't write but it is 16
    REQUIRE(f.bitdepth() == 16);
    REQUIRE_FALSE(f.ten_giga().has_value());
    REQUIRE_FALSE(f.threshold_energy().has_value());
    REQUIRE_FALSE(f.threshold_energy_all().has_value());
    REQUIRE_FALSE(f.subexptime().has_value());
    REQUIRE_FALSE(f.subperiod().has_value());
    REQUIRE_FALSE(f.quad().has_value());
    REQUIRE(f.number_of_rows() == 512);
    REQUIRE_FALSE(f.rate_corrections().has_value());
    REQUIRE_FALSE(f.adc_mask().has_value()); 
    REQUIRE_FALSE(f.analog_flag()); 
    REQUIRE_FALSE(f.analog_samples().has_value()); 
    REQUIRE_FALSE(f.digital_flag()); 
    REQUIRE_FALSE(f.digital_samples().has_value()); 
    REQUIRE_FALSE(f.dbit_offset().has_value()); 
    REQUIRE_FALSE(f.dbit_list().has_value()); 
    REQUIRE_FALSE(f.transceiver_mask().has_value()); 
    REQUIRE_FALSE(f.transceiver_flag()); 
    REQUIRE_FALSE(f.transceiver_samples().has_value()); 
    REQUIRE_FALSE(f.roi().has_value()); 
    REQUIRE_FALSE(f.counter_mask().has_value()); 
    REQUIRE_FALSE(f.exptime_array().has_value()); 
    REQUIRE_FALSE(f.gate_delay_array().has_value()); 
    REQUIRE_FALSE(f.gates().has_value()); 
    REQUIRE_FALSE(f.additional_json_header().has_value()); 
    REQUIRE(f.frames_in_file() == 5); 
    REQUIRE(f.n_modules() == 2);
}

TEST_CASE("Parse a single module jungfrau master file in .h5 format", "[.integration][.hdf5]") {
    auto fpath = test_data_path() / "hdf5" / "virtual" / "jungfrau" /
                 "single_module_master_2.h5";
    REQUIRE(std::filesystem::exists(fpath));
    Hdf5MasterFile f(fpath);

    REQUIRE(f.version() == "6.6");
    // "Timestamp": "Tue Feb 20 08:28:24 2024",
    REQUIRE(f.detector_type() == DetectorType::Jungfrau);
    REQUIRE(f.timing_mode() == TimingMode::Auto);
    REQUIRE(f.geometry().col == 1);
    REQUIRE(f.geometry().row == 1);
    REQUIRE(f.image_size_in_bytes() == 1048576);
    REQUIRE(f.pixels_x() == 1024);
    REQUIRE(f.pixels_y() == 512);
    REQUIRE(f.max_frames_per_file() == 10000);
    REQUIRE(f.frame_discard_policy() == FrameDiscardPolicy::NoDiscard);
    REQUIRE(f.frame_padding() == 1);
    REQUIRE(f.scan_parameters()->enabled() == false);
    REQUIRE(f.total_frames_expected() == 5);
    REQUIRE(f.exptime() == std::chrono::microseconds(10));
    REQUIRE(f.period() == std::chrono::milliseconds(2));
    REQUIRE_FALSE(f.burst_mode().has_value()); 
    REQUIRE(f.number_of_udp_interfaces() == 1);
    // Jungfrau doesn't write but it is 16
    REQUIRE(f.bitdepth() == 16);
    REQUIRE_FALSE(f.ten_giga().has_value());
    REQUIRE_FALSE(f.threshold_energy().has_value());
    REQUIRE_FALSE(f.threshold_energy_all().has_value());
    REQUIRE_FALSE(f.subexptime().has_value());
    REQUIRE_FALSE(f.subperiod().has_value());
    REQUIRE_FALSE(f.quad().has_value());
    REQUIRE(f.number_of_rows() == 512);
    REQUIRE_FALSE(f.rate_corrections().has_value());
    REQUIRE_FALSE(f.adc_mask().has_value()); 
    REQUIRE_FALSE(f.analog_flag()); 
    REQUIRE_FALSE(f.analog_samples().has_value()); 
    REQUIRE_FALSE(f.digital_flag()); 
    REQUIRE_FALSE(f.digital_samples().has_value()); 
    REQUIRE_FALSE(f.dbit_offset().has_value()); 
    REQUIRE_FALSE(f.dbit_list().has_value()); 
    REQUIRE_FALSE(f.transceiver_mask().has_value()); 
    REQUIRE_FALSE(f.transceiver_flag()); 
    REQUIRE_FALSE(f.transceiver_samples().has_value()); 
    REQUIRE_FALSE(f.roi().has_value()); 
    REQUIRE_FALSE(f.counter_mask().has_value()); 
    REQUIRE_FALSE(f.exptime_array().has_value()); 
    REQUIRE_FALSE(f.gate_delay_array().has_value()); 
    REQUIRE_FALSE(f.gates().has_value()); 
    REQUIRE_FALSE(f.additional_json_header().has_value()); 
    REQUIRE(f.frames_in_file() == 5); 
    REQUIRE(f.n_modules() == 1);
}

TEST_CASE("Parse a mythen3 master file in .h5 format", "[.integration][.hdf5]") {
    auto fpath = test_data_path() / "hdf5" / "virtual" / "mythen3" /
                 "one_module_master_0.h5";
    REQUIRE(std::filesystem::exists(fpath));
    Hdf5MasterFile f(fpath);

    REQUIRE(f.version() == "6.7");
    // "Timestamp": "Tue Feb 20 08:28:24 2024",
    REQUIRE(f.detector_type() == DetectorType::Mythen3);
    REQUIRE(f.timing_mode() == TimingMode::Auto);
    REQUIRE(f.geometry().col == 1);
    REQUIRE(f.geometry().row == 1);
    REQUIRE(f.image_size_in_bytes() == 15360);
    REQUIRE(f.pixels_x() == 3840);
    REQUIRE(f.pixels_y() == 1);
    REQUIRE(f.max_frames_per_file() == 10000);
    REQUIRE(f.frame_discard_policy() == FrameDiscardPolicy::NoDiscard);
    REQUIRE(f.frame_padding() == 1);
    REQUIRE(f.scan_parameters()->enabled() == false);
    REQUIRE(f.total_frames_expected() == 1);
    REQUIRE_FALSE(f.exptime().has_value());
    REQUIRE(f.period() == std::chrono::nanoseconds(0));
    REQUIRE_FALSE(f.burst_mode().has_value()); 
    REQUIRE_FALSE(f.number_of_udp_interfaces().has_value());
    REQUIRE(f.bitdepth() == 32);
    REQUIRE(f.ten_giga() == 1);
    REQUIRE_FALSE(f.threshold_energy().has_value());
    REQUIRE(ToString(f.threshold_energy_all()) == "[-1, -1, -1]");
    REQUIRE_FALSE(f.subexptime().has_value());
    REQUIRE_FALSE(f.subperiod().has_value());
    REQUIRE_FALSE(f.quad().has_value());
    REQUIRE_FALSE(f.number_of_rows().has_value());
    REQUIRE_FALSE(f.rate_corrections().has_value());
    REQUIRE_FALSE(f.adc_mask().has_value()); 
    REQUIRE_FALSE(f.analog_flag()); 
    REQUIRE_FALSE(f.analog_samples().has_value()); 
    REQUIRE_FALSE(f.digital_flag()); 
    REQUIRE_FALSE(f.digital_samples().has_value()); 
    REQUIRE_FALSE(f.dbit_offset().has_value()); 
    REQUIRE_FALSE(f.dbit_list().has_value()); 
    REQUIRE_FALSE(f.transceiver_mask().has_value()); 
    REQUIRE_FALSE(f.transceiver_flag()); 
    REQUIRE_FALSE(f.transceiver_samples().has_value()); 
    REQUIRE_FALSE(f.roi().has_value()); 
    REQUIRE(f.counter_mask() == 0x7); 
    REQUIRE(ToString(f.exptime_array()) == "[0.1s, 0.1s, 0.1s]"); 
    REQUIRE(ToString(f.gate_delay_array()) == "[0ns, 0ns, 0ns]"); 
    REQUIRE(f.gates() == 1); 
    REQUIRE_FALSE(f.additional_json_header().has_value()); 
    REQUIRE(f.frames_in_file() == 1); 
    REQUIRE(f.n_modules() == 1);
}
