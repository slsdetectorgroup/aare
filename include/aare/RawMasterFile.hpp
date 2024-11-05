#pragma once
#include "aare/defs.hpp"
#include <filesystem>
#include <fmt/format.h>
#include <fstream>
#include <optional>

#include <nlohmann/json.hpp>
using json = nlohmann::json;

namespace aare {

class RawFileNameComponents {
    std::filesystem::path m_base_path{};
    std::string m_base_name{};
    std::string m_ext{};
    int m_file_index{}; // TODO! is this measurement_index?

  public:
    RawFileNameComponents(const std::filesystem::path &fname);
    std::filesystem::path master_fname() const {
        return m_base_path /
               fmt::format("{}_master_{}{}", m_base_name, m_file_index, m_ext);
    }

    std::filesystem::path data_fname(size_t mod_id, size_t file_id) const{
        return m_base_path / fmt::format("{}_d{}_f{}_{}.raw", m_base_name,
                                         mod_id, file_id, m_file_index);
    }

    const std::filesystem::path &base_path() const { return m_base_path; }
    const std::string &base_name() const { return m_base_name; }
    const std::string &ext() const { return m_ext; }
    int file_index() const { return m_file_index; }
};


/**
 * @brief Class for parsing a master file either in our .json format or the old .raw format
 */
class RawMasterFile {
    RawFileNameComponents m_fnc;
    std::string m_version;
    DetectorType m_type;
    TimingMode m_timing_mode;

    size_t m_image_size_in_bytes;
    size_t m_frames_in_file;
    size_t m_pixels_y;
    size_t m_pixels_x;
    size_t m_bitdepth;

    size_t m_max_frames_per_file;
    uint32_t m_adc_mask;
    FrameDiscardPolicy m_frame_discard_policy;
    size_t m_frame_padding;

    std::optional<size_t> m_analog_samples;
    std::optional<size_t> m_digital_samples;

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

    std::optional<size_t> analog_samples() const;
    std::optional<size_t> digital_samples() const;

    

  private:
    void parse_json(const std::filesystem::path &fpath);
    void parse_raw(const std::filesystem::path &fpath);
};

} // namespace aare