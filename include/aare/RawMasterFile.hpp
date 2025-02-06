#pragma once
#include "aare/defs.hpp"
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
    std::string m_dac;
    int m_start = 0;
    int m_stop = 0;
    int m_step = 0;
    //TODO! add settleTime, requires string to time conversion

  public:
    ScanParameters(const std::string &par);
    ScanParameters() = default;
    ScanParameters(const ScanParameters &) = default;
    ScanParameters &operator=(const ScanParameters &) = default;
    ScanParameters(ScanParameters &&) = default;
    int start() const;
    int stop() const;
    int step() const;
    const std::string &dac() const;
    bool enabled() const;
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

    xy m_geometry{};

    size_t m_max_frames_per_file{};
    // uint32_t m_adc_mask{}; // TODO! implement reading
    FrameDiscardPolicy m_frame_discard_policy{};
    size_t m_frame_padding{};

    // TODO! should these be bool?
    uint8_t m_analog_flag{};
    uint8_t m_digital_flag{};
    uint8_t m_transceiver_flag{};

    ScanParameters m_scan_parameters;

    std::optional<size_t> m_analog_samples;
    std::optional<size_t> m_digital_samples;
    std::optional<size_t> m_transceiver_samples;
    std::optional<size_t> m_number_of_rows;
    std::optional<uint8_t> m_quad;

    std::optional<ROI> m_roi;


  public:
    RawMasterFile(const std::filesystem::path &fpath);

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
    const FrameDiscardPolicy &frame_discard_policy() const;

    size_t total_frames_expected() const;
    xy geometry() const;

    std::optional<size_t> analog_samples() const;
    std::optional<size_t> digital_samples() const;
    std::optional<size_t> transceiver_samples() const;
    std::optional<size_t> number_of_rows() const;
    std::optional<uint8_t> quad() const;


    std::optional<ROI> roi() const;


    ScanParameters scan_parameters() const;

  private:
    void parse_json(const std::filesystem::path &fpath);
    void parse_raw(const std::filesystem::path &fpath);
};

} // namespace aare