#pragma once
#include "aare/defs.hpp"
#include "aare/scan_parameters.hpp"

#include "H5Cpp.h"
#include <filesystem>
#include <fmt/format.h>
#include <fstream>
#include <optional>

namespace aare {

using ns = std::chrono::nanoseconds;

/**
 * @brief Class for parsing a master file either in our .json format or the old
 * .Hdf5 format
 */
class Hdf5MasterFile {
    std::filesystem::path m_file_name{};
    std::string m_version;
    DetectorType m_type;
    TimingMode m_timing_mode;
    xy m_geometry{};
    int m_image_size_in_bytes{};
    int m_pixels_y{};
    int m_pixels_x{};
    int m_max_frames_per_file{};
    FrameDiscardPolicy m_frame_discard_policy{};
    int m_frame_padding{};
    std::optional<ScanParameters> m_scan_parameters{};
    size_t m_total_frames_expected{};
    std::optional<ns> m_exptime{};
    std::optional<ns> m_period{};
    std::optional<BurstMode> m_burst_mode{};
    std::optional<int> m_number_of_udp_interfaces{};
    int m_bitdepth{};
    std::optional<bool> m_ten_giga{};
    std::optional<int> m_threshold_energy{};
    std::optional<std::vector<int>> m_threshold_energy_all{};
    std::optional<ns> m_subexptime{};
    std::optional<ns> m_subperiod{};
    std::optional<bool> m_quad{};
    std::optional<int> m_number_of_rows{};
    std::optional<std::vector<size_t>> m_rate_corrections{};
    uint32_t m_adc_mask{};
    bool m_analog_flag{};
    std::optional<int> m_analog_samples{};
    bool m_digital_flag{};
    std::optional<int> m_digital_samples{};
    std::optional<int> m_dbit_offset{};
    std::optional<size_t> m_dbit_list{};
    std::optional<int> m_transceiver_mask{};
    bool m_transceiver_flag{};
    std::optional<int> m_transceiver_samples{};
    // g1 roi - will not be implemented?
    std::optional<ROI> m_roi{};
    std::optional<int> m_counter_mask{};
    std::optional<std::vector<ns>> m_exptime_array{};
    std::optional<std::vector<ns>> m_gate_delay_array{};
    std::optional<int> m_gates{};
    std::optional<std::map<std::string, std::string>>
        m_additional_json_header{};
    size_t m_frames_in_file{};

    // TODO! should these be bool?

  public:
    Hdf5MasterFile(const std::filesystem::path &fpath);

    std::filesystem::path file_name() const;

    const std::string &version() const; //!< For example "7.2"
    const DetectorType &detector_type() const;
    const TimingMode &timing_mode() const;
    xy geometry() const;
    int image_size_in_bytes() const;
    int pixels_y() const;
    int pixels_x() const;
    int max_frames_per_file() const;
    const FrameDiscardPolicy &frame_discard_policy() const;
    int frame_padding() const;
    std::optional<ScanParameters> scan_parameters() const;
    size_t total_frames_expected() const;
    std::optional<ns> exptime() const;
    std::optional<ns> period() const;
    std::optional<BurstMode> burst_mode() const;
    std::optional<int> number_of_udp_interfaces() const;
    int bitdepth() const;
    std::optional<bool> ten_giga() const;
    std::optional<int> threshold_energy() const;
    std::optional<std::vector<int>> threshold_energy_all() const;
    std::optional<ns> subexptime() const;
    std::optional<ns> subperiod() const;
    std::optional<bool> quad() const;
    std::optional<int> number_of_rows() const;
    std::optional<std::vector<size_t>> rate_corrections() const;
    std::optional<uint32_t> adc_mask() const;
    bool analog_flag() const;
    std::optional<int> analog_samples() const;
    bool digital_flag() const;
    std::optional<int> digital_samples() const;
    std::optional<int> dbit_offset() const;
    std::optional<size_t> dbit_list() const;
    std::optional<int> transceiver_mask() const;
    bool transceiver_flag() const;
    std::optional<int> transceiver_samples() const;
    // g1 roi - will not be implemented?
    std::optional<ROI> roi() const;
    std::optional<int> counter_mask() const;
    std::optional<std::vector<ns>> exptime_array() const;
    std::optional<std::vector<ns>> gate_delay_array() const;
    std::optional<int> gates() const;
    std::optional<std::map<std::string, std::string>>
    additional_json_header() const;
    size_t frames_in_file() const;
    size_t n_modules() const;

  private:
    static const std::string metadata_group_name;
    void parse_acquisition_metadata(const std::filesystem::path &fpath);

    template <typename T>
    T h5_read_scalar_dataset(const H5::DataSet &dataset,
                             const H5::DataType &data_type);

    template <typename T>
    T h5_get_scalar_dataset(const H5::H5File &file,
                            const std::string &dataset_name);
};

template <>
std::string Hdf5MasterFile::h5_read_scalar_dataset<std::string>(
    const H5::DataSet &dataset, const H5::DataType &data_type);
} // namespace aare