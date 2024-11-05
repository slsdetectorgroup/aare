#include "aare/RawMasterFile.hpp"

namespace aare {

RawFileNameComponents::RawFileNameComponents(
    const std::filesystem::path &fname) {
    m_base_path = fname.parent_path();
    m_base_name = fname.stem();
    m_ext = fname.extension();

    // parse file index
    try {
        auto pos = m_base_name.rfind('_');
        m_file_index = std::stoi(m_base_name.substr(pos + 1));
    } catch (const std::invalid_argument &e) {
        throw std::runtime_error(LOCATION + "Could not parse file index");
    }

    // remove master from base name
    auto pos = m_base_name.find("_master_");
    if (pos != std::string::npos) {
        m_base_name.erase(pos);
    } else {
        throw std::runtime_error(LOCATION +
                                 "Could not find _master_ in file name");
    }
}

RawMasterFile::RawMasterFile(const std::filesystem::path &fpath)
    : m_fnc(fpath) {
    if (!std::filesystem::exists(fpath)) {
        throw std::runtime_error(LOCATION + " File does not exist");
    }
    if (m_fnc.ext() == ".json") {
        parse_json(fpath);
    } else if (m_fnc.ext() == ".raw") {
        parse_raw(fpath);
    } else {
        throw std::runtime_error(LOCATION + "Unsupported file type");
    }
}

std::filesystem::path RawMasterFile::data_fname(size_t mod_id,
                                                size_t file_id) const {
    return m_fnc.data_fname(mod_id, file_id);
}

const std::string &RawMasterFile::version() const { return m_version; }
const DetectorType &RawMasterFile::detector_type() const { return m_type; }
const TimingMode &RawMasterFile::timing_mode() const { return m_timing_mode; }
size_t RawMasterFile::image_size_in_bytes() const {
    return m_image_size_in_bytes;
}
size_t RawMasterFile::frames_in_file() const { return m_frames_in_file; }
size_t RawMasterFile::pixels_y() const { return m_pixels_y; }
size_t RawMasterFile::pixels_x() const { return m_pixels_x; }
size_t RawMasterFile::max_frames_per_file() const {
    return m_max_frames_per_file;
}
size_t RawMasterFile::bitdepth() const { return m_bitdepth; }
size_t RawMasterFile::frame_padding() const { return m_frame_padding; }
const FrameDiscardPolicy &RawMasterFile::frame_discard_policy() const {
    return m_frame_discard_policy;
}

// optional values, these may or may not be present in the master file
// and are therefore modeled as std::optional
std::optional<size_t> RawMasterFile::analog_samples() const {
    return m_analog_samples;
}
std::optional<size_t> RawMasterFile::digital_samples() const {
    return m_digital_samples;
}

void RawMasterFile::parse_json(const std::filesystem::path &fpath) {
    std::ifstream ifs(fpath);
    json j;
    ifs >> j;
    double v = j["Version"];
    m_version = fmt::format("{:.1f}", v);

    m_type = StringTo<DetectorType>(j["Detector Type"].get<std::string>());
    m_timing_mode = StringTo<TimingMode>(j["Timing Mode"].get<std::string>());

    m_image_size_in_bytes = j["Image Size in bytes"];
    m_frames_in_file = j["Frames in File"];
    m_pixels_y = j["Pixels"]["y"];
    m_pixels_x = j["Pixels"]["x"];

    m_max_frames_per_file = j["Max Frames Per File"];

    // Not all detectors write the bitdepth but in case
    // its not there it is 16
    try {
        m_bitdepth = j.at("Dynamic Range");
    } catch (const json::out_of_range &e) {
        m_bitdepth = 16;
    }

    m_frame_padding = j["Frame Padding"];
    m_frame_discard_policy = StringTo<FrameDiscardPolicy>(
        j["Frame Discard Policy"].get<std::string>());

    try {
        m_analog_samples = j.at("Analog Samples");
    } catch (const json::out_of_range &e) {
        // m_analog_samples = 0;
    }
    // try{
    //     std::string adc_mask = j.at("ADC Mask");
    //     m_adc_mask = std::stoul(adc_mask, nullptr, 16);
    // }catch (const json::out_of_range &e) {
    //     m_adc_mask = 0;
    // }

    try {
        m_digital_samples = j.at("Digital Samples");
    } catch (const json::out_of_range &e) {
        // m_digital_samples = 0;
    }

    // //Update detector type for Moench
    // //TODO! How does this work with old .raw master files?
    // if (m_type == DetectorType::Moench && m_analog_samples == 0 &&
    // m_subfile_rows == 400) {
    //     m_type = DetectorType::Moench03;
    // }else if (m_type == DetectorType::Moench && m_subfile_rows == 400 &&
    // m_analog_samples == 5000) {
    //     m_type = DetectorType::Moench03_old;
    // }

    // //Here we know we have a ChipTestBoard file update the geometry?
    // //TODO! Carry on information about digtial, and transceivers
    // if (m_type == DetectorType::ChipTestBoard) {
    //    subfile_rows = 1;
    //    subfile_cols = m_analog_samples*__builtin_popcount(m_adc_mask);
    // }

    // // only Eiger had quad
    // if (m_type == DetectorType::Eiger) {
    //     quad = (j["Quad"] == 1);
    // }

    // m_geometry = {j["Geometry"]["y"], j["Geometry"]["x"]};
}
void RawMasterFile::parse_raw(const std::filesystem::path &fpath) {
    throw std::runtime_error("Not implemented");
}
} // namespace aare