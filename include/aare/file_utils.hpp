#pragma once
#include "aare/defs.hpp"
#include <filesystem>
#include <fmt/format.h>
#include <fstream>

#include <nlohmann/json.hpp>
using json = nlohmann::json;

namespace aare {
bool is_master_file(const std::filesystem::path &fpath);


struct FileNameComponents {
    std::filesystem::path base_path{};
    std::string base_name{};
    std::string ext{};
    int findex{};
    bool valid{false}; // TODO! how do we do error handling?

    std::filesystem::path master_fname() const {
        return base_path /
               fmt::format("{}_master_{}{}", base_name, findex, ext);
    }

    std::filesystem::path data_fname(size_t mod_id, size_t file_id) {
        return base_path / fmt::format("{}_d{}_f{}_{}.raw", base_name, file_id,
                                       mod_id, findex);
    }
};

FileNameComponents parse_fname(const std::filesystem::path &fname);

class MasterFile {
    FileNameComponents m_fnc;
    std::string m_version;
    DetectorType m_type;
    TimingMode m_timing_mode;
    size_t m_total_frames;
    size_t m_subfile_rows;
    size_t m_subfile_cols;
    size_t m_bitdepth;
    size_t m_analog_samples;
    size_t m_digital_samples;
    size_t m_max_frames_per_file;
    uint32_t m_adc_mask;
public:
    MasterFile(const std::filesystem::path &fpath) {
        m_fnc = parse_fname(fpath);

        

        std::ifstream ifs(fpath);
        json j;
        ifs >> j;
        double v = j["Version"];
        m_version = fmt::format("{:.1f}", v);

        m_type = StringTo<DetectorType>(j["Detector// Type"].get<std::string>()); 
        m_timing_mode =
        StringTo<TimingMode>(j["Timing Mode"].get<std::string>());
        m_total_frames = j["Frames in File"];
        m_subfile_rows = j["Pixels"]["y"];
        m_subfile_cols = j["Pixels"]["x"];
        m_max_frames_per_file = j["Max Frames Per File"];
        try {
            m_bitdepth = j.at("Dynamic Range");
        } catch (const json::out_of_range &e) {
            m_bitdepth = 16;
        }

        try {
            m_analog_samples = j.at("Analog Samples");
        }catch (const json::out_of_range &e) {
            m_analog_samples = 0;
        }
        try{
            std::string adc_mask = j.at("ADC Mask");
            m_adc_mask = std::stoul(adc_mask, nullptr, 16);
        }catch (const json::out_of_range &e) {
            m_adc_mask = 0;
        }

        try {
            m_digital_samples = j.at("Digital Samples");
            }catch (const json::out_of_range &e) {
            m_digital_samples = 0;
        }

        //Update detector type for Moench
        //TODO! How does this work with old .raw master files?
        if (m_type == DetectorType::Moench && m_analog_samples == 0 &&
        m_subfile_rows == 400) {
            m_type = DetectorType::Moench03;
        }else if (m_type == DetectorType::Moench && m_subfile_rows == 400 &&
        m_analog_samples == 5000) {
            m_type = DetectorType::Moench03_old;
        }

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

};



} // namespace aare