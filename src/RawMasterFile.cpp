#include "aare/RawMasterFile.hpp"

namespace aare {


RawFileNameComponents::RawFileNameComponents(const std::filesystem::path &fname) {
    m_base_path = fname.parent_path();
    m_base_name = fname.stem();
    m_ext = fname.extension();

    //parse file index
    try {
        auto pos = m_base_name.rfind('_');
        m_file_index = std::stoi(m_base_name.substr(pos + 1));
    } catch (const std::invalid_argument &e) {
        throw std::runtime_error(LOCATION + "Could not parse file index");
    }

    //remove master from base name
    auto pos = m_base_name.find("_master_");
    if (pos != std::string::npos) {
        m_base_name.erase(pos);
    }else{
        throw std::runtime_error(LOCATION + "Could not find _master_ in file name");
    }
}

void RawMasterFile::parse_json(const std::filesystem::path &fpath) {
        std::ifstream ifs(fpath);
        json j;
        ifs >> j;
        double v = j["Version"];
        m_version = fmt::format("{:.1f}", v);

        m_type = StringTo<DetectorType>(j["Detector Type"].get<std::string>()); 
        m_timing_mode =
        StringTo<TimingMode>(j["Timing Mode"].get<std::string>());

        m_image_size_in_bytes = j["Image Size in bytes"];
        m_frames_in_file = j["Frames in File"];
        m_pixels_y = j["Pixels"]["y"];
        m_pixels_x = j["Pixels"]["x"];

        m_max_frames_per_file = j["Max Frames Per File"];

        //Not all detectors write the bitdepth but in case
        //its not there it is 16
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
        }catch (const json::out_of_range &e) {
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
        }catch (const json::out_of_range &e) {
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