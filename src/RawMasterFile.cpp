// SPDX-License-Identifier: MPL-2.0
#include "aare/RawMasterFile.hpp"
#include "aare/RawFile.hpp"
#include "aare/logger.hpp"
#include <sstream>

#include "to_string.hpp"

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

std::filesystem::path RawFileNameComponents::data_fname(size_t mod_id,
                                                        size_t file_id) const {

    std::string fmt = "{}_d{}_f{}_{}.raw";
    // Before version X we used to name the data files f000000000000
    if (m_old_scheme) {
        fmt = "{}_d{}_f{:012}_{}.raw";
    }
    return m_base_path / fmt::format(fmt::runtime(fmt), m_base_name, mod_id,
                                     file_id, m_file_index);
}

void RawFileNameComponents::set_old_scheme(bool old_scheme) {
    m_old_scheme = old_scheme;
}

const std::filesystem::path &RawFileNameComponents::base_path() const {
    return m_base_path;
}
const std::string &RawFileNameComponents::base_name() const {
    return m_base_name;
}
const std::string &RawFileNameComponents::ext() const { return m_ext; }
int RawFileNameComponents::file_index() const { return m_file_index; }

ScanParameters::ScanParameters(const bool enabled, const DACIndex dac,
                               const int start, const int stop, const int step,
                               const int64_t settleTime)
    : m_enabled(enabled), m_dac(dac), m_start(start), m_stop(stop),
      m_step(step), m_settleTime(settleTime) {}

// "[enabled\ndac dac 4\nstart 500\nstop 2200\nstep 5\nsettleTime 100us\n]"
ScanParameters::ScanParameters(const std::string &par) {
    std::istringstream iss(par.substr(1, par.size() - 2));
    std::string line;
    while (std::getline(iss, line)) {
        if (line == "enabled") {
            m_enabled = true;
        } else if (line.find("dac") != std::string::npos) {
            m_dac = string_to<DACIndex>(line.substr(4));
        } else if (line.find("start") != std::string::npos) {
            m_start = std::stoi(line.substr(6));
        } else if (line.find("stop") != std::string::npos) {
            m_stop = std::stoi(line.substr(5));
        } else if (line.find("step") != std::string::npos) {
            m_step = std::stoi(line.substr(5));
        }
    }
}

int ScanParameters::start() const { return m_start; }
int ScanParameters::stop() const { return m_stop; }
void ScanParameters::increment_stop() { m_stop += 1; }
int ScanParameters::step() const { return m_step; }
DACIndex ScanParameters::dac() const { return m_dac; }
bool ScanParameters::enabled() const { return m_enabled; }
int64_t ScanParameters::settleTime() const { return m_settleTime; }

RawMasterFile::RawMasterFile(const std::filesystem::path &fpath)
    : m_fnc(fpath) {
    if (!std::filesystem::exists(fpath)) {
        throw std::runtime_error(fmt::format("{} File does not exist: {}",
                                             LOCATION, fpath.string()));
    }

    std::ifstream ifs(fpath);
    if (m_fnc.ext() == ".json") {
        parse_json(ifs);
    } else if (m_fnc.ext() == ".raw") {
        parse_raw(ifs);
    } else {
        throw std::runtime_error(LOCATION + "Unsupported file type");
    }

    m_geometry = DetectorGeometry(m_detector_layout, m_pixels_x, m_pixels_y,
                                  m_udp_interfaces_per_module, m_quad);

    if (m_quad == 1 && m_udp_port_types.has_value()) {
        m_udp_port_types.value() = {"top", "bottom"};
    }

    if (m_disabled_udp_ports.has_value()) {
        // ROI takes precedence over disabled UDP ports, if both are defined in
        // the master file
        if (!complete_ROI(m_rois, m_geometry)) {
            LOG(logWARNING)
                << "ROI and disabled UDP ports defined in master file. ROI "
                   "will be used and disabled UDP ports will be ignored.";
        } else {
            update_rois_from_disabled_udp_ports();
        }
    }

    m_ROI_geometries.reserve(m_rois.size());

    for (const auto &roi : m_rois) {
        m_ROI_geometries.push_back(ROIGeometry(roi, m_geometry));
    }
}

RawMasterFile::RawMasterFile(std::istream &is, const std::string &fname)
    : m_fnc(fname) {

    if (m_fnc.ext() == ".json") {
        parse_json(is);
    } else if (m_fnc.ext() == ".raw") {
        parse_raw(is);
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

std::optional<uint8_t> RawMasterFile::counter_mask() const {
    return m_counter_mask;
}

xy RawMasterFile::detector_layout() const { return m_detector_layout; }

const DetectorGeometry &RawMasterFile::geometry() const { return m_geometry; }

const std::vector<ROIGeometry> &RawMasterFile::roi_geometries() const {
    return m_ROI_geometries;
}

size_t RawMasterFile::n_modules() const {
    return m_detector_layout.row * m_detector_layout.col;
}

xy RawMasterFile::udp_interfaces_per_module() const {
    return m_udp_interfaces_per_module;
}

uint8_t RawMasterFile::quad() const { return m_quad; }

// optional values, these may or may not be present in the master file
// and are therefore modeled as std::optional
std::optional<size_t> RawMasterFile::analog_samples() const {
    return m_analog_samples;
}
std::optional<size_t> RawMasterFile::digital_samples() const {
    return m_digital_samples;
}

std::optional<size_t> RawMasterFile::transceiver_samples() const {
    return m_transceiver_samples;
}

ScanParameters RawMasterFile::scan_parameters() const {
    return m_scan_parameters;
}

std::optional<std::vector<std::string>> RawMasterFile::udp_port_types() const {
    return m_udp_port_types;
}

std::optional<std::vector<size_t>> RawMasterFile::disabled_udp_ports() const {
    return m_disabled_udp_ports;
}

ROI RawMasterFile::roi() const {

    if (m_rois.empty()) {
        throw std::runtime_error(LOCATION + "Zero ROIs in metadata.");
    }

    if (m_rois.size() > 1) {
        throw std::runtime_error(LOCATION +
                                 "Multiple ROIs present, use rois() method.");
    } else {
        return m_rois.at(0);
    }
}

std::vector<ROI> RawMasterFile::rois() const { return m_rois; }

ReadoutMode RawMasterFile::get_reading_mode() const {

    if (m_type != DetectorType::ChipTestBoard &&
        m_type != DetectorType::Xilinx_ChipTestBoard) {
        LOG(TLogLevel::logINFO)
            << "reading mode is only available for CTB detectors.";
        return ReadoutMode::UNKNOWN;
    }

    if (m_analog_flag && m_digital_flag) {
        return ReadoutMode::ANALOG_AND_DIGITAL;
    } else if (m_analog_flag) {
        return ReadoutMode::ANALOG_ONLY;
    } else if (m_digital_flag && m_transceiver_flag) {
        return ReadoutMode::DIGITAL_AND_TRANSCEIVER;
    } else if (m_digital_flag) {
        return ReadoutMode::DIGITAL_ONLY;
    } else if (m_transceiver_flag) {
        return ReadoutMode::TRANSCEIVER_ONLY;
    } else {
        return ReadoutMode::UNKNOWN;
    }
}

void RawMasterFile::parse_json(std::istream &is) {
    json j;
    is >> j;

    double v = j["Version"];
    m_version = fmt::format("{:.1f}", v);

    m_type = string_to<DetectorType>(j["Detector Type"].get<std::string>());
    m_timing_mode = string_to<TimingMode>(j["Timing Mode"].get<std::string>());

    m_detector_layout = {
        j["Geometry"]["y"],
        j["Geometry"]["x"]}; // TODO: isnt it only available for
                             // version > 7.1?
                             // - try block default should be 1x1

    m_image_size_in_bytes =
        v < 8.0 ? j["Image Size in bytes"] : j["Image Size"];

    m_frames_in_file = j["Frames in File"];
    m_pixels_y = j["Pixels"]["y"];
    m_pixels_x = j["Pixels"]["x"];

    m_max_frames_per_file = j["Max Frames Per File"];

    // Before v8.0 we had Exptime instead of Exposure Time
    // Mythen3 uses 3 exposure times and is not handled at the moment
    if (j.contains("Exptime") && j["Exptime"].is_string()) {
        m_exptime = string_to<std::chrono::nanoseconds>(
            j["Exptime"].get<std::string>());
    }
    if (j.contains("Exposure Time") && j["Exposure Time"].is_string()) {
        m_exptime = string_to<std::chrono::nanoseconds>(
            j["Exposure Time"].get<std::string>());
    }

    // Before v8.0 we had Period instead of Acquisition Period
    if (j.contains("Period") && j["Period"].is_string()) {
        m_period =
            string_to<std::chrono::nanoseconds>(j["Period"].get<std::string>());
    }
    if (j.contains("Acquisition Period") &&
        j["Acquisition Period"].is_string()) {
        m_period = string_to<std::chrono::nanoseconds>(
            j["Acquisition Period"].get<std::string>());
    }

    // TODO! Not valid for CTB but not changing api right now!
    // Not all detectors write the bitdepth but in case
    // its not there it is 16
    if (j.contains("Dynamic Range") && j["Dynamic Range"].is_number()) {
        m_bitdepth = j["Dynamic Range"];
    } else {
        m_bitdepth = 16;
    }
    m_total_frames_expected = j["Total Frames"];

    m_frame_padding = j["Frame Padding"];
    m_frame_discard_policy = string_to<FrameDiscardPolicy>(
        j["Frame Discard Policy"].get<std::string>());

    if (j.contains("Number of rows") && j["Number of rows"].is_number()) {
        m_number_of_rows = j["Number of rows"];
    }

    // ----------------------------------------------------------------
    // Special treatment of analog flag because of Moench03.
    // Before SW 8.0.0 (NOT json file version v8!) Moench03 had Analog Samples
    // but no Analog Flag. Therefore we need to set analog flag to true and try
    // to read analog samples The detector type is later set depending on analog
    // samples and number of pixels.

    if (m_type == DetectorType::Moench) {
        if (auto it = j.find("Analog Samples"); it != j.end()) {
            m_analog_flag = true;
            m_analog_samples = it->get<size_t>();
        } else {
            m_analog_flag = false;
        }
    } else {
        if (auto it = j.find("Analog Flag"); it != j.end()) {
            m_analog_flag = static_cast<bool>(it->get<int>());
            if (m_analog_flag) {
                m_analog_samples = j.at("Analog Samples");
            }
        } else {
            m_analog_flag = false;
        }
    }
    //-----------------------------------------------------------------

    try {
        m_quad = j.at("Quad");
    } catch (const json::out_of_range &e) {
        // keep the optional empty
    }
    // try{
    //     std::string adc_mask = j.at("ADC Mask");
    //     m_adc_mask = std::stoul(adc_mask, nullptr, 16);
    // }catch (const json::out_of_range &e) {
    //     m_adc_mask = 0;
    // }
    try {
        bool digital_flag = static_cast<bool>(j.at("Digital Flag").get<int>());
        if (digital_flag) {
            m_digital_samples = j.at("Digital Samples");
        }
    } catch (const json::out_of_range &e) {
        // keep the optional empty
    }
    try {
        m_transceiver_flag =
            static_cast<bool>(j.at("Transceiver Flag").get<int>());
        if (m_transceiver_flag) {
            m_transceiver_samples = j.at("Transceiver Samples");
        }
    } catch (const json::out_of_range &e) {
        // keep the optional empty
    }
    try {
        if (v < 8.0) {
            std::string scan_parameters = j.at("Scan Parameters");
            m_scan_parameters = ScanParameters(scan_parameters);
        } else {
            auto json_obj = j.at("Scan Parameters");
            m_scan_parameters = ScanParameters(
                json_obj.at("enable").get<int>(),
                static_cast<DACIndex>(json_obj.at("dacInd").get<int>()),
                json_obj.at("start offset").get<int>(),
                json_obj.at("stop offset").get<int>(),
                json_obj.at("step size").get<int>(),
                json_obj.at("dac settle time ns").get<int>());
        }
        if (v < 7.21) {
            m_scan_parameters
                .increment_stop(); // adjust for endpoint being included
        }
    } catch (const json::out_of_range &e) {
        // not a scan
    }
    try {
        auto json_list_obj = j.at("UDP Ports Type");
        m_udp_port_types.emplace();
        for (auto &elem : json_list_obj) {
            m_udp_port_types.value().push_back(elem);
        }
    } catch (const json::out_of_range &e) {
        // leave the optional empty
    }
    try {
        auto json_list_obj = j.at("UDP Ports Disabled");
        m_disabled_udp_ports.emplace();
        for (auto &elem : json_list_obj) {
            m_disabled_udp_ports.value().push_back(static_cast<size_t>(elem));
        }
    } catch (const json::out_of_range &e) {
        // leave the optional empty
    }
    try {
        m_udp_interfaces_per_module = {j.at("Number of UDP Interfaces"), 1};
    } catch (const json::out_of_range &e) {
        if (m_type == DetectorType::Eiger && m_quad == 1)
            m_udp_interfaces_per_module = {2, 1};
        else if (m_type == DetectorType::Eiger) {
            m_udp_interfaces_per_module = {1, 2};
        }
    }
    try {
        if (v < 8.0) {
            auto obj = j.at("Receiver Roi");
            if (obj.at("xmin") != 4294967295 || obj.at("xmax") != 4294967295 ||
                obj.at("ymin") != 4294967295 || obj.at("ymax") != 4294967295) {
                // Handle Mythen
                if (obj.at("ymin") == -1 && obj.at("ymax") == -1) {
                    obj.at("ymin") = 0;
                    obj.at("ymax") = 0;
                }
                m_rois.push_back({static_cast<ssize_t>(obj.at("xmin")),
                                  static_cast<ssize_t>(obj.at("xmax")) + 1,
                                  static_cast<ssize_t>(obj.at("ymin")),
                                  static_cast<ssize_t>(obj.at("ymax")) + 1});
            } else {
                // fill ROI with full detector size if not present in master
                // file
                m_rois.push_back({0, static_cast<ssize_t>(m_pixels_x), 0,
                                  static_cast<ssize_t>(m_pixels_y)});
            }
        } else {
            auto obj = j.at("Receiver Rois");
            for (auto &elem : obj) {
                // handle Mythen
                if (elem.at("ymin") == -1 && elem.at("ymax") == -1) {
                    elem.at("ymin") = 0;
                    elem.at("ymax") = 0;
                }

                m_rois.push_back({static_cast<ssize_t>(elem.at("xmin")),
                                  static_cast<ssize_t>(elem.at("xmax")) + 1,
                                  static_cast<ssize_t>(elem.at("ymin")),
                                  static_cast<ssize_t>(elem.at("ymax")) + 1});
            }
        }

    } catch (const json::out_of_range &e) {
        // fill ROI with full detector size if not present in master file
        m_rois.push_back({0, static_cast<ssize_t>(m_pixels_x), 0,
                          static_cast<ssize_t>(m_pixels_y)});
    }

    if (j.contains("Counter Mask")) {
        if (j["Counter Mask"].is_number())
            m_counter_mask = j["Counter Mask"];
        else if (j["Counter Mask"].is_string())
            m_counter_mask =
                std::stoi(j["Counter Mask"].get<std::string>(), nullptr, 16);
    }

    // Update detector type for Moench
    // TODO! How does this work with old .raw master files?
    if (m_type == DetectorType::Moench && !m_analog_samples &&
        m_pixels_y == 400) {
        m_type = DetectorType::Moench03;
    } else if (m_type == DetectorType::Moench && m_pixels_y == 400 &&
               m_analog_samples == 5000) {
        m_type = DetectorType::Moench03_old;
    }
}
void RawMasterFile::parse_raw(std::istream &is) {
    for (std::string line; std::getline(is, line);) {
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

                // TODO!: How old versions can we handle?
                auto v = std::stod(value);

                // TODO! figure out exactly when we did the change
                // This enables padding of f to 12 digits
                if (v < 4.0)
                    m_fnc.set_old_scheme(true);

            } else if (key == "TimeStamp") {

            } else if (key == "Detector Type") {
                m_type = string_to<DetectorType>(value);
                if (m_type == DetectorType::Moench) {
                    m_type = DetectorType::Moench03_old;
                }
            } else if (key == "Timing Mode") {
                m_timing_mode = string_to<TimingMode>(value);
            } else if (key == "Image Size") {
                m_image_size_in_bytes = std::stoi(value);
            } else if (key == "Frame Padding") {
                m_frame_padding = std::stoi(value);
                // } else if (key == "Frame Discard Policy"){
                //     m_frame_discard_policy =
                //     StringTo<FrameDiscardPolicy>(value);
                // } else if (key == "Number of rows"){
                //     m_number_of_rows = std::stoi(value);
            } else if (key == "Analog Flag") {
                m_analog_flag = static_cast<bool>(std::stoi(value));
            } else if (key == "Digital Flag") {
                m_digital_flag = static_cast<bool>(std::stoi(value));

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
            } else if (key == "row") {
                pos = value.find('p');
                m_pixels_y = std::stoi(value.substr(0, pos));
            } else if (key == "col") {
                pos = value.find('p');
                m_pixels_x = std::stoi(value.substr(0, pos));
            } else if (key == "Total Frames") {
                m_total_frames_expected = std::stoi(value);
            } else if (key == "Exptime") {
                m_exptime = string_to<std::chrono::nanoseconds>(value);
            } else if (key == "Period") {
                m_period = string_to<std::chrono::nanoseconds>(value);
            } else if (key == "Dynamic Range") {
                m_bitdepth = std::stoi(value);
            } else if (key == "Quad") {
                m_quad = std::stoi(value);
            } else if (key == "Max Frames Per File") {
                m_max_frames_per_file = std::stoi(value);
            } else if (key == "Max. Frames Per File") {
                // Version 3.0 way of writing it
                m_max_frames_per_file = std::stoi(value);
            } else if (key == "Geometry") {
                pos = value.find(',');
                m_detector_layout = {
                    static_cast<uint32_t>(std::stoi(value.substr(1, pos))),
                    static_cast<uint32_t>(std::stoi(value.substr(pos + 1)))};
            } else if (key == "Number of UDP Interfaces") {
                m_udp_interfaces_per_module = {
                    static_cast<uint32_t>(std::stoi(value)), 1};
            }
        }
    }

    if (m_type == DetectorType::Eiger && m_quad == 1) {
        m_udp_interfaces_per_module = {2, 1};
    } else if (m_type == DetectorType::Eiger) {
        m_udp_interfaces_per_module = {1, 2};
    }

    if (m_pixels_x == 400 && m_pixels_y == 400) {
        m_type = DetectorType::Moench03_old;
    }

    if (m_detector_layout.col == 0 && m_detector_layout.row == 0) {
        retrieve_geometry();
        LOG(TLogLevel::logWARNING)
            << "No geometry found in master file. Retrieved geometry of "
            << m_detector_layout.row << " x " << m_detector_layout.col << "\n ";
    }

    // TODO! Read files and find actual frames
    if (m_frames_in_file == 0)
        m_frames_in_file = m_total_frames_expected;
}

void RawMasterFile::retrieve_geometry() {
    uint32_t module_index = 0;
    uint16_t rows = 0;
    uint16_t cols = 0;
    // TODO use case for Eiger

    while (std::filesystem::exists(data_fname(module_index, 0))) {

        auto header = RawFile::read_header(data_fname(module_index, 0));

        rows = std::max(rows, header.row);
        cols = std::max(cols, header.column);

        ++module_index;
    }
    ++rows;
    ++cols;

    m_detector_layout = {rows, cols};
}

/**
 * @brief Update ROIs from disabled UDP ports
 */
void RawMasterFile::update_rois_from_disabled_udp_ports() {

    const size_t num_udp_port_types = m_udp_port_types.value().size();

    auto disabled_ports = m_disabled_udp_ports.value();

    size_t first_port = disabled_ports[0] % num_udp_port_types;

    bool all_ports_equal =
        std::all_of(disabled_ports.begin(), disabled_ports.end(),
                    [&num_udp_port_types, first_port](size_t &port) {
                        return port % num_udp_port_types == first_port;
                    });

    m_rois.clear(); // remove global roi

    const ssize_t udp_ports_per_module =
        m_geometry.udp_interfaces_per_module().col *
        m_geometry.udp_interfaces_per_module().row;

    bool port_disabled_for_all_modules =
        disabled_ports.size() ==
        m_geometry.modules_x() * m_geometry.modules_y() / udp_ports_per_module;

    if (all_ports_equal && port_disabled_for_all_modules) {

        if (m_udp_port_types.value()[first_port] == "left") {
            const size_t num_rois =
                m_geometry.modules_x() / udp_ports_per_module;
            m_rois.resize(num_rois);

            const ssize_t pixels_per_module_x =
                m_geometry.pixels_x() / m_geometry.modules_x();
            std::generate(
                m_rois.begin(), m_rois.end(),
                [n = 0, this, pixels_per_module_x,
                 udp_ports_per_module]() mutable {
                    ssize_t idx = n++;
                    return ROI{
                        idx * udp_ports_per_module * pixels_per_module_x +
                            pixels_per_module_x,
                        idx * udp_ports_per_module * pixels_per_module_x +
                            2 * pixels_per_module_x,
                        0, static_cast<ssize_t>(m_geometry.pixels_y())};
                });
        }
        if (m_udp_port_types.value()[first_port] == "right") {
            const size_t num_rois =
                m_geometry.modules_x() / udp_ports_per_module;
            m_rois.resize(num_rois);

            const ssize_t pixels_per_module_x =
                m_geometry.pixels_x() / m_geometry.modules_x();
            std::generate(
                m_rois.begin(), m_rois.end(),
                [n = 0, this, pixels_per_module_x,
                 udp_ports_per_module]() mutable {
                    ssize_t idx = n++;
                    return ROI{idx * udp_ports_per_module * pixels_per_module_x,
                               idx * udp_ports_per_module *
                                       pixels_per_module_x +
                                   pixels_per_module_x,
                               0, static_cast<ssize_t>(m_geometry.pixels_y())};
                });
        }
        if (m_udp_port_types.value()[first_port] == "top") {
            size_t num_rois = m_geometry.modules_y() / udp_ports_per_module;
            m_rois.resize(num_rois);
            // assumes euclidean coordinate system with origin at bottom
            // left corner of the detector
            const ssize_t pixels_per_module_y =
                m_geometry.pixels_y() / m_geometry.modules_y();
            std::generate(
                m_rois.begin(), m_rois.end(),
                [n = 0, this, pixels_per_module_y,
                 udp_ports_per_module]() mutable {
                    ssize_t idx = n++;
                    return ROI{0, static_cast<ssize_t>(m_geometry.pixels_x()),
                               idx * udp_ports_per_module * pixels_per_module_y,
                               idx * udp_ports_per_module *
                                       pixels_per_module_y +
                                   pixels_per_module_y};
                });
        }
        if (m_udp_port_types.value()[first_port] == "bottom") {
            size_t num_rois = m_geometry.modules_y() / udp_ports_per_module;
            m_rois.resize(num_rois);
            const ssize_t pixels_per_module_y =
                m_geometry.pixels_y() / m_geometry.modules_y();
            std::generate(
                m_rois.begin(), m_rois.end(),
                [n = 0, this, pixels_per_module_y,
                 udp_ports_per_module]() mutable {
                    ssize_t idx = n++;
                    return ROI{
                        0, static_cast<ssize_t>(m_geometry.pixels_x()),
                        idx * udp_ports_per_module * pixels_per_module_y +
                            pixels_per_module_y,
                        idx * udp_ports_per_module * pixels_per_module_y +
                            2 * pixels_per_module_y};
                });
        }
    } else {
        // iterate over all ports and create ROIs for each disabled port
        LOG(logDEBUG) << "Creating ROIs from disabled UDP ports";
        // get the enabled ones:
        std::vector<size_t> enabled_ports(m_geometry.n_modules());
        std::iota(enabled_ports.begin(), enabled_ports.end(), 0);
        std::for_each(disabled_ports.begin(), disabled_ports.end(),
                      [&enabled_ports](size_t &port) {
                          enabled_ports.erase(std::remove(enabled_ports.begin(),
                                                          enabled_ports.end(),
                                                          port),
                                              enabled_ports.end());
                      });

        m_rois.reserve(enabled_ports.size());
        for (const auto enabled_port : enabled_ports) {

            auto module_geometry =
                m_geometry.get_module_geometries(enabled_port);
            m_rois.push_back(
                ROI{module_geometry.origin_x,
                    module_geometry.origin_x + module_geometry.width,
                    module_geometry.origin_y,
                    module_geometry.origin_y + module_geometry.height});
        }

        if (m_udp_port_types == std::vector<std::string>{"left", "right"}) {
            m_rois = merge_consecutive_rois<false, true>(m_rois);
        } else if (m_udp_port_types ==
                       std::vector<std::string>{"bottom", "top"} ||
                   m_udp_port_types ==
                       std::vector<std::string>{"top", "bottom"}) {
            m_rois = merge_consecutive_rois<true, false>(m_rois);
        } else {
            throw std::runtime_error(LOCATION + "Unsupported UDP port types");
        }
    }
}

} // namespace aare
