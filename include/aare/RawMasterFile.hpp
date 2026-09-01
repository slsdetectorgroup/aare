// SPDX-License-Identifier: MPL-2.0
#pragma once
#include "aare/DetectorGeometry.hpp"
#include "aare/ROI.hpp"
#include <algorithm>
#include <chrono>
#include <filesystem>
#include <fmt/format.h>
#include <fstream>
#include <optional>

#include <nlohmann/json.hpp>
using json = nlohmann::json;

namespace aare {

/**
 * @brief Implementation used in RawMasterFile to parse the file name
 */
class RawFileNameComponents {
    bool m_old_scheme{false};
    std::filesystem::path m_base_path{};
    std::string m_base_name{};
    std::string m_ext{};
    int m_file_index{}; // TODO! is this measurement_index?

  public:
    RawFileNameComponents(const std::filesystem::path &fname);

    /// @brief Get the filename including path of the master file.
    /// (i.e. what was passed in to the constructor))
    std::filesystem::path master_fname() const;

    /// @brief Get the filename including path of the data file.
    /// @param mod_id module id run_d[module_id]_f0_0
    /// @param file_id file id run_d0_f[file_id]_0
    std::filesystem::path data_fname(size_t mod_id, size_t file_id) const;

    const std::filesystem::path &base_path() const;
    const std::string &base_name() const;
    const std::string &ext() const;
    int file_index() const;
    void set_old_scheme(bool old_scheme);
};

class ScanParameters {
    bool m_enabled = false;
    DACIndex m_dac{};
    int m_start = 0;
    int m_stop = 0;
    int m_step = 0;
    int64_t m_settleTime = 0; // [ns]

  public:
    ScanParameters(const std::string &par);
    ScanParameters(const bool enabled, const DACIndex dac, const int start,
                   const int stop, const int step, const int64_t settleTime);
    ScanParameters() = default;
    ScanParameters(const ScanParameters &) = default;
    ScanParameters &operator=(const ScanParameters &) = default;
    ScanParameters(ScanParameters &&) = default;
    int start() const;
    int stop() const;
    int step() const;
    DACIndex dac() const;
    bool enabled() const;
    int64_t settleTime() const;
    void increment_stop();
};

/**
 * @brief Class for parsing a master file either in our .json format or the old
 * .raw format
 */
class RawMasterFile {
    RawFileNameComponents m_fnc;
    std::string m_version;
    DetectorType m_type;
    TimingMode m_timing_mode;

    size_t m_image_size_in_bytes{};
    size_t m_frames_in_file{};
    size_t m_total_frames_expected{};
    size_t m_pixels_y{};
    size_t m_pixels_x{};
    size_t m_bitdepth{};
    uint8_t m_quad = 0;

    std::optional<std::chrono::nanoseconds> m_exptime;
    std::chrono::nanoseconds m_period{0};

    /// @brief modules in x and y direction
    xy m_detector_layout{};
    xy m_udp_interfaces_per_module{1, 1};

    size_t m_max_frames_per_file{};
    // uint32_t m_adc_mask{}; // TODO! implement reading
    FrameDiscardPolicy m_frame_discard_policy{};
    size_t m_frame_padding{};

    // TODO! should these be bool?
    bool m_analog_flag{};
    bool m_digital_flag{};
    bool m_transceiver_flag{};

    ScanParameters m_scan_parameters;

    std::optional<size_t> m_analog_samples;
    std::optional<size_t> m_digital_samples;
    std::optional<size_t> m_transceiver_samples;
    std::optional<size_t> m_number_of_rows;
    std::optional<uint8_t> m_counter_mask;

    /// @brief index of disabled UDP ports - index relative to UDP_port_types
    std::optional<std::vector<size_t>> m_disabled_udp_ports{};

    /// @brief udp port types
    std::optional<std::vector<std::string>>
        m_udp_port_types{}; // TODO: UDPPortType? - string_to conversion?

    /// @brief ROIs defined in master file or derived from disabled UDP ports
    std::vector<ROI> m_rois;

    /// @brief Detector geometry - geometry for each module
    DetectorGeometry m_geometry{};

    /// @brief ROI geometries
    std::vector<ROIGeometry> m_ROI_geometries;

  public:
    RawMasterFile(const std::filesystem::path &fpath);
    RawMasterFile(std::istream &is, const std::string &fname); // for testing

    std::filesystem::path data_fname(size_t mod_id, size_t file_id) const;

    const std::string &version() const; //!< For example "7.2"
    const DetectorType &detector_type() const;
    const TimingMode &timing_mode() const;
    size_t image_size_in_bytes() const;
    size_t frames_in_file() const;
    size_t pixels_y() const;
    size_t pixels_x() const;
    size_t max_frames_per_file() const;
    size_t bitdepth() const;
    size_t frame_padding() const;
    xy udp_interfaces_per_module() const;
    const FrameDiscardPolicy &frame_discard_policy() const;

    size_t total_frames_expected() const;
    xy detector_layout() const;
    size_t n_modules() const;
    uint8_t quad() const;

    const DetectorGeometry &geometry() const;

    const std::vector<ROIGeometry> &roi_geometries() const;

    ReadoutMode get_reading_mode() const;

    std::optional<size_t> analog_samples() const;
    std::optional<size_t> digital_samples() const;
    std::optional<size_t> transceiver_samples() const;
    std::optional<size_t> number_of_rows() const;
    std::optional<uint8_t> counter_mask() const;

    /// @brief Get the types of UDP ports
    /// @return Optional vector of UDP port types as strings (only present for
    /// masterfile version >= 8.1)
    std::optional<std::vector<std::string>> udp_port_types() const;

    /// @brief Get the indices of disabled UDP ports
    /// @return Optional vector of indices of disabled UDP ports (only present
    /// for masterfile version >= 8.1)
    std::optional<std::vector<size_t>> disabled_udp_ports() const;

    std::vector<ROI> rois() const;

    /// @brief get roi for the case of a single ROI
    /// @return ROI object (complete ROI if no roi present in master file)
    ROI roi() const;

    ScanParameters scan_parameters() const;

    std::optional<std::chrono::nanoseconds> exptime() const {
        return m_exptime;
    }
    std::chrono::nanoseconds period() const { return m_period; }

  private:
    void parse_json(std::istream &is);
    void parse_raw(std::istream &is);
    void update_rois_from_disabled_udp_ports();
    void retrieve_geometry();
};

} // namespace aare