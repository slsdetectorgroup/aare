#include "aare/RawMasterFile.hpp"

namespace aare {

RawFileNameComponents::RawFileNameComponents(
    const std::filesystem::path &fname) {
    m_base_path = fname.parent_path();
    m_base_name = fname.stem();
    m_ext = fname.extension();

    if (m_ext != ".json" && m_ext != ".raw") {
        throw std::runtime_error(LOCATION +
                                 "Unsupported file type. (only .json or .raw)");
    }

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

std::filesystem::path RawFileNameComponents::master_fname() const {
        return m_base_path /
               fmt::format("{}_master_{}{}", m_base_name, m_file_index, m_ext);
    }

std::filesystem::path RawFileNameComponents::data_fname(size_t mod_id, size_t file_id) const{
        return m_base_path / fmt::format("{}_d{}_f{}_{}.raw", m_base_name,
                                         mod_id, file_id, m_file_index);
    }

const std::filesystem::path& RawFileNameComponents::base_path() const { return m_base_path; }
    const std::string& RawFileNameComponents::base_name() const { return m_base_name; }
    const std::string& RawFileNameComponents::ext() const { return m_ext; }
    int RawFileNameComponents::file_index() const { return m_file_index; }


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

size_t RawMasterFile::total_frames_expected() const {
    return m_total_frames_expected;
}

std::optional<size_t> RawMasterFile::number_of_rows() const {
    return m_number_of_rows;
}

xy RawMasterFile::geometry() const { return m_geometry; }

std::optional<uint8_t> RawMasterFile::quad() const { return m_quad; }

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

    m_geometry = {j["Geometry"]["y"], j["Geometry"]["x"]};

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
    m_total_frames_expected = j["Total Frames"];

    m_frame_padding = j["Frame Padding"];
    m_frame_discard_policy = StringTo<FrameDiscardPolicy>(
        j["Frame Discard Policy"].get<std::string>());

    try {
        m_number_of_rows = j.at("Number of rows");
    } catch (const json::out_of_range &e) {
        // keep the optional empty
    }

    try {
        int analog_flag = j.at("Analog Flag");
        if (analog_flag) {
            m_analog_samples = j.at("Analog Samples");
        }

    } catch (const json::out_of_range &e) {
        // keep the optional empty
    }

    try{
        m_quad = j.at("Quad");
    }catch (const json::out_of_range &e) {
        // keep the optional empty
    }
    // try{
    //     std::string adc_mask = j.at("ADC Mask");
    //     m_adc_mask = std::stoul(adc_mask, nullptr, 16);
    // }catch (const json::out_of_range &e) {
    //     m_adc_mask = 0;
    // }

    try {
        int digital_flag = j.at("Digital Flag");
        if (digital_flag) {
            m_digital_samples = j.at("Digital Samples");
        }
    } catch (const json::out_of_range &e) {
        // keep the optional empty
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

    std::ifstream ifs(fpath);
    for (std::string line; std::getline(ifs, line);) {
        if (line == "#Frame Header")
            break;
        auto pos = line.find(':');
        auto key_pos = pos;
        while (key_pos != std::string::npos && std::isspace(line[--key_pos]))
            ;
        if (key_pos != std::string::npos) {
            auto key = line.substr(0, key_pos + 1);
            auto value = line.substr(pos + 2);
            // do the actual parsing
            if (key == "Version") {
                m_version = value;
            } else if (key == "TimeStamp") {

            } else if (key == "Detector Type") {
                m_type = StringTo<DetectorType>(value);
            } else if (key == "Timing Mode") {
                m_timing_mode = StringTo<TimingMode>(value);
            } else if (key == "Image Size") {
                m_image_size_in_bytes = std::stoi(value);
            } else if (key == "Frame Padding"){
                m_frame_padding = std::stoi(value);
            // } else if (key == "Frame Discard Policy"){
            //     m_frame_discard_policy = StringTo<FrameDiscardPolicy>(value);
            // } else if (key == "Number of rows"){
            //     m_number_of_rows = std::stoi(value);
            } else if (key == "Analog Flag") {
                m_analog_flag = std::stoi(value);
            } else if (key == "Digital Flag") {
                m_digital_flag = std::stoi(value);

            } else if (key == "Analog Samples") {
                if (m_analog_flag == 1) {
                    m_analog_samples = std::stoi(value);
                }
            } else if (key == "Digital Samples") {
                if (m_digital_flag == 1) {
                    m_digital_samples = std::stoi(value);
                }
            } else if (key == "Frames in File") {
                m_frames_in_file = std::stoi(value);
            // } else if (key == "ADC Mask") {
            //     m_adc_mask = std::stoi(value, nullptr, 16);
            } else if (key == "Pixels") {
                // Total number of pixels cannot be found yet looking at
                // submodule
                pos = value.find(',');
                m_pixels_x = std::stoi(value.substr(1, pos));
                m_pixels_y = std::stoi(value.substr(pos + 1));
            } else if (key == "Total Frames") {
                m_total_frames_expected = std::stoi(value);
            } else if (key == "Dynamic Range") {
                m_bitdepth = std::stoi(value);
            } else if (key == "Quad") {
                m_quad = std::stoi(value);
            } else if (key == "Max Frames Per File") {
                m_max_frames_per_file = std::stoi(value);
            } else if (key == "Geometry") {
                pos = value.find(',');
                m_geometry = {
                    static_cast<uint32_t>(std::stoi(value.substr(1, pos))),
                    static_cast<uint32_t>(std::stoi(value.substr(pos + 1)))};
            }
        }
    }
}
} // namespace aare