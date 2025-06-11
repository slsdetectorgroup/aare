#include "aare/Hdf5MasterFile.hpp"
#include "aare/logger.hpp"
#include <iomanip>
#include <sstream>
namespace aare {

Hdf5MasterFile::Hdf5MasterFile(const std::filesystem::path &fpath)
    : m_file_name(fpath) {
    if (!std::filesystem::exists(fpath)) {
        throw std::runtime_error(LOCATION + " File does not exist");
    }
    parse_acquisition_metadata(fpath);
}

std::filesystem::path Hdf5MasterFile::file_name() const {
    return m_file_name;
}

const std::string &Hdf5MasterFile::version() const { return m_version; }
const DetectorType &Hdf5MasterFile::detector_type() const { return m_type; }
const TimingMode &Hdf5MasterFile::timing_mode() const { return m_timing_mode; }
xy Hdf5MasterFile::geometry() const { return m_geometry; }
int Hdf5MasterFile::image_size_in_bytes() const {
    return m_image_size_in_bytes;
}
int Hdf5MasterFile::pixels_y() const { return m_pixels_y; }
int Hdf5MasterFile::pixels_x() const { return m_pixels_x; }
int Hdf5MasterFile::max_frames_per_file() const {
    return m_max_frames_per_file;
}
const FrameDiscardPolicy &Hdf5MasterFile::frame_discard_policy() const {
    return m_frame_discard_policy;
}
int Hdf5MasterFile::frame_padding() const { return m_frame_padding; }
std::optional<ScanParameters> Hdf5MasterFile::scan_parameters() const {
    return m_scan_parameters;
}
size_t Hdf5MasterFile::total_frames_expected() const {
    return m_total_frames_expected;
}
std::optional<ns> Hdf5MasterFile::exptime() const { return m_exptime; }
std::optional<ns> Hdf5MasterFile::period() const { return m_period; }
std::optional<BurstMode> Hdf5MasterFile::burst_mode() const {
    return m_burst_mode;
}
std::optional<int> Hdf5MasterFile::number_of_udp_interfaces() const {
    return m_number_of_udp_interfaces;
}
int Hdf5MasterFile::bitdepth() const { return m_bitdepth; }
std::optional<bool> Hdf5MasterFile::ten_giga() const { return m_ten_giga; }
std::optional<int> Hdf5MasterFile::threshold_energy() const {
    return m_threshold_energy;
}
std::optional<std::vector<int>> Hdf5MasterFile::threshold_energy_all() const {
    return m_threshold_energy_all;
}
std::optional<ns> Hdf5MasterFile::subexptime() const { return m_subexptime; }
std::optional<ns> Hdf5MasterFile::subperiod() const { return m_subperiod; }
std::optional<bool> Hdf5MasterFile::quad() const { return m_quad; }
std::optional<int> Hdf5MasterFile::number_of_rows() const {
    return m_number_of_rows;
}
std::optional<std::vector<size_t>> Hdf5MasterFile::rate_corrections() const {
    return m_rate_corrections;
}
std::optional<uint32_t> Hdf5MasterFile::adc_mask() const { return m_adc_mask; }
bool Hdf5MasterFile::analog_flag() const { return m_analog_flag; }
std::optional<int> Hdf5MasterFile::analog_samples() const {
    return m_analog_samples;
}
bool Hdf5MasterFile::digital_flag() const { return m_digital_flag; }
std::optional<int> Hdf5MasterFile::digital_samples() const {
    return m_digital_samples;
}
std::optional<int> Hdf5MasterFile::dbit_offset() const { return m_dbit_offset; }
std::optional<size_t> Hdf5MasterFile::dbit_list() const { return m_dbit_list; }
std::optional<int> Hdf5MasterFile::transceiver_mask() const {
    return m_transceiver_mask;
}
bool Hdf5MasterFile::transceiver_flag() const { return m_transceiver_flag; }
std::optional<int> Hdf5MasterFile::transceiver_samples() const {
    return m_transceiver_samples;
}
// g1 roi
std::optional<ROI> Hdf5MasterFile::roi() const { return m_roi; }
std::optional<int> Hdf5MasterFile::counter_mask() const {
    return m_counter_mask;
}
std::optional<std::vector<ns>> Hdf5MasterFile::exptime_array() const {
    return m_exptime_array;
}
std::optional<std::vector<ns>> Hdf5MasterFile::gate_delay_array() const {
    return m_gate_delay_array;
}
std::optional<int> Hdf5MasterFile::gates() const { return m_gates; }
std::optional<std::map<std::string, std::string>>
Hdf5MasterFile::additional_json_header() const {
    return m_additional_json_header;
}
size_t Hdf5MasterFile::frames_in_file() const { return m_frames_in_file; }
size_t Hdf5MasterFile::n_modules() const {
    return m_geometry.row * m_geometry.col;
}

// optional values, these may or may not be present in the master file
// and are therefore modeled as std::optional

const std::string Hdf5MasterFile::metadata_group_name =
    "/entry/instrument/detector/";

template <typename T>
T Hdf5MasterFile::h5_read_scalar_dataset(const H5::DataSet &dataset,
                                         const H5::DataType &data_type) {
    T value;
    dataset.read(&value, data_type);
    return value;
}

template <>
std::string Hdf5MasterFile::h5_read_scalar_dataset<std::string>(
    const H5::DataSet &dataset, const H5::DataType &data_type) {
    size_t size = data_type.getSize();
    std::vector<char> buffer(size + 1, 0);
    dataset.read(buffer.data(), data_type);
    return std::string(buffer.data());
}

template <typename T>
T Hdf5MasterFile::h5_get_scalar_dataset(const H5::H5File &file,
                                        const std::string &dataset_name) {
    H5::DataSet dataset = file.openDataSet(dataset_name);
    H5::DataSpace dataspace = dataset.getSpace();
    if (dataspace.getSimpleExtentNdims() != 0) {
        throw std::runtime_error(LOCATION + "Expected " + dataset_name +
                                 " to be a scalar dataset");
    }
    H5::DataType data_type = dataset.getDataType();
    return h5_read_scalar_dataset<T>(dataset, data_type);
}

void Hdf5MasterFile::parse_acquisition_metadata(
    const std::filesystem::path &fpath) {
    try {
        H5::H5File file(fpath, H5F_ACC_RDONLY);

        // Attribute - version
        double dVersion{0.0};
        {
            H5::Attribute attr = file.openAttribute("version");
            H5::DataType attr_type = attr.getDataType();
            attr.read(attr_type, &dVersion);
            std::ostringstream oss;
            oss << std::fixed << std::setprecision(1) << dVersion;
            m_version = oss.str();
            LOG(logDEBUG) << "Version: " << m_version;
        }

        // Scalar Dataset
        H5::Exception::dontPrint();

        // Detector Type
        m_type = StringTo<DetectorType>(h5_get_scalar_dataset<std::string>(
            file, std::string(metadata_group_name + "Detector Type")));
        LOG(logDEBUG) << "Detector Type: " << ToString(m_type);

        // Timing Mode
        m_timing_mode = StringTo<TimingMode>(h5_get_scalar_dataset<std::string>(
            file, std::string(metadata_group_name + "Timing Mode")));
        LOG(logDEBUG) << "Timing Mode: " << ToString(m_timing_mode);

        // Geometry
        m_geometry.row = h5_get_scalar_dataset<int>(
            file, std::string(metadata_group_name + "Geometry in y axis"));
        m_geometry.col = h5_get_scalar_dataset<int>(
            file, std::string(metadata_group_name + "Geometry in x axis"));
        LOG(logDEBUG) << "Geometry: " << m_geometry.to_string();

        // Image Size
        m_image_size_in_bytes = h5_get_scalar_dataset<int>(
            file, std::string(metadata_group_name + "Image Size"));
        LOG(logDEBUG) << "Image size: " << m_image_size_in_bytes;

        // Pixels y
        m_pixels_y = h5_get_scalar_dataset<int>(
            file,
            std::string(metadata_group_name + "Number of pixels in y axis"));
        LOG(logDEBUG) << "Pixels in y: " << m_pixels_y;

        // Pixels x
        m_pixels_x = h5_get_scalar_dataset<int>(
            file,
            std::string(metadata_group_name + "Number of pixels in x axis"));
        LOG(logDEBUG) << "Pixels in x: " << m_pixels_x;

        // Max Frames Per File
        m_max_frames_per_file = h5_get_scalar_dataset<int>(
            file, std::string(metadata_group_name + "Maximum frames per file"));
        LOG(logDEBUG) << "Max frames per File: " << m_max_frames_per_file;

        // Frame Discard Policy
        m_frame_discard_policy =
            StringTo<FrameDiscardPolicy>(h5_get_scalar_dataset<std::string>(
                file,
                std::string(metadata_group_name + "Frame Discard Policy")));
        LOG(logDEBUG) << "Frame Discard Policy: "
                      << ToString(m_frame_discard_policy);

        // Frame Padding
        m_frame_padding = h5_get_scalar_dataset<int>(
            file, std::string(metadata_group_name + "Frame Padding"));
        LOG(logDEBUG) << "Frame Padding: " << m_frame_padding;

        // Scan Parameters
        try {
            std::string scan_parameters = h5_get_scalar_dataset<std::string>(
                file, std::string(metadata_group_name + "Scan Parameters"));
            m_scan_parameters = ScanParameters(scan_parameters);
            if (dVersion < 6.61) {
                m_scan_parameters
                    ->increment_stop(); // adjust for endpoint being included
            }
            LOG(logDEBUG) << "Scan Parameters: " << ToString(m_scan_parameters);
        } catch (H5::FileIException &e) {
            // keep the optional empty
        }

        // Total Frames Expected
        m_total_frames_expected = h5_get_scalar_dataset<size_t>(
            file, std::string(metadata_group_name + "Total Frames"));
        LOG(logDEBUG) << "Total Frames: " << m_total_frames_expected;

        // Exptime
        try {
            m_exptime = StringTo<ns>(h5_get_scalar_dataset<std::string>(
                file, std::string(metadata_group_name + "Exposure Time")));
            LOG(logDEBUG) << "Exptime: " << ToString(m_exptime);
        } catch (H5::FileIException &e) {
            // keep the optional empty
        }

        // Period
        try {
            m_period = StringTo<ns>(h5_get_scalar_dataset<std::string>(
                file, std::string(metadata_group_name + "Acquisition Period")));
            LOG(logDEBUG) << "Period: " << ToString(m_period);
        } catch (H5::FileIException &e) {
            // keep the optional empty
        }

        // burst mode
        try {
            m_burst_mode =
                StringTo<BurstMode>(h5_get_scalar_dataset<std::string>(
                    file, std::string(metadata_group_name + "Burst Mode")));
            LOG(logDEBUG) << "Burst Mode: " << ToString(m_burst_mode);
        } catch (H5::FileIException &e) {
            // keep the optional empty
        }

        // Number of UDP Interfaces
        // Not all detectors write the Number of UDP Interfaces but in case
        try {
            m_number_of_udp_interfaces = h5_get_scalar_dataset<int>(
                file,
                std::string(metadata_group_name + "Number of UDP Interfaces"));
            LOG(logDEBUG) << "Number of UDP Interfaces: "
                          << m_number_of_udp_interfaces;
        } catch (H5::FileIException &e) {
            // keep the optional empty
        }

        // Bit Depth
        // Not all detectors write the bitdepth but in case
        // its not there it is 16
        try {
            m_bitdepth = h5_get_scalar_dataset<int>(
                file, std::string(metadata_group_name + "Dynamic Range"));
            LOG(logDEBUG) << "Bit Depth: " << m_bitdepth;
        } catch (H5::FileIException &e) {
            m_bitdepth = 16;
        }

        // Ten Giga
        try {
            m_ten_giga = h5_get_scalar_dataset<bool>(
                file, std::string(metadata_group_name + "Ten Giga Enable"));
            LOG(logDEBUG) << "Ten Giga Enable: " << m_ten_giga;
        } catch (H5::FileIException &e) {
            // keep the optional empty
        }

        // Threshold Energy
        try {
            m_threshold_energy = h5_get_scalar_dataset<int>(
                file, std::string(metadata_group_name + "Threshold Energy"));
            LOG(logDEBUG) << "Threshold Energy: " << m_threshold_energy;
        } catch (H5::FileIException &e) {
            // keep the optional empty
        }

        // Threshold All Energy
        try {
            m_threshold_energy_all =
                StringTo<std::vector<int>>(h5_get_scalar_dataset<std::string>(
                    file,
                    std::string(metadata_group_name + "Threshold Energies")));
            LOG(logDEBUG) << "Threshold Energies: "
                          << ToString(m_threshold_energy_all);
        } catch (H5::FileIException &e) {
            std::cout << "No Threshold Energies found in file: " << fpath
                      << std::endl;
            // keep the optional empty
        }

        // Subexptime
        try {
            m_subexptime = StringTo<ns>(h5_get_scalar_dataset<std::string>(
                file, std::string(metadata_group_name + "Sub Exposure Time")));
            LOG(logDEBUG) << "Subexptime: " << ToString(m_subexptime);
        } catch (H5::FileIException &e) {
            // keep the optional empty
        }

        // Subperiod
        try {
            m_subperiod = StringTo<ns>(h5_get_scalar_dataset<std::string>(
                file, std::string(metadata_group_name + "Sub Period")));
            LOG(logDEBUG) << "Subperiod: " << ToString(m_subperiod);
        } catch (H5::FileIException &e) {
            // keep the optional empty
        }

        // Quad
        try {
            m_quad = h5_get_scalar_dataset<bool>(
                file, std::string(metadata_group_name + "Quad"));
            LOG(logDEBUG) << "Quad: " << m_quad;
        } catch (H5::FileIException &e) {
            // keep the optional empty
        }

        // Number of Rows
        // Not all detectors write the Number of rows but in case
        try {
            m_number_of_rows = h5_get_scalar_dataset<int>(
                file, std::string(metadata_group_name + "Number of rows"));
            LOG(logDEBUG) << "Number of rows: " << m_number_of_rows;
        } catch (H5::FileIException &e) {
            // keep the optional empty
        }

        // Rate Corrections
        try {
            m_rate_corrections = StringTo<std::vector<size_t>>(
                h5_get_scalar_dataset<std::string>(
                    file,
                    std::string(metadata_group_name + "Rate Corrections")));
            LOG(logDEBUG) << "Rate Corrections: "
                          << ToString(m_rate_corrections);
        } catch (H5::FileIException &e) {
            // keep the optional empty
        }

        // ADC Mask
        try {
            m_adc_mask = h5_get_scalar_dataset<uint32_t>(
                file, std::string(metadata_group_name + "ADC Mask"));
        } catch (H5::FileIException &e) {
            // keep the optional empty
        }
        LOG(logDEBUG) << "ADC Mask: " << m_adc_mask;

        // Analog Flag
        // ----------------------------------------------------------------
        // Special treatment of analog flag because of Moench03
        try {
            m_analog_flag = h5_get_scalar_dataset<uint8_t>(
                file, std::string(metadata_group_name + "Analog Flag"));
            LOG(logDEBUG) << "Analog Flag: " << m_analog_flag;
        } catch (H5::FileIException &e) {
            // if it doesn't work still set it to one
            // to try to decode analog samples (Old Moench03)
            m_analog_flag = 1;
        }

        // Analog Samples
        try {
            if (m_analog_flag) {
                m_analog_samples = h5_get_scalar_dataset<int>(
                    file, std::string(metadata_group_name + "Analog Samples"));
                LOG(logDEBUG) << "Analog Samples: " << m_analog_samples;
            }
        } catch (H5::FileIException &e) {
            // keep the optional empty
            // and set analog flag to 0
            m_analog_flag = false;
        }
        //-----------------------------------------------------------------

        // Digital Flag, Digital Samples
        try {
            m_digital_flag = h5_get_scalar_dataset<bool>(
                file, std::string(metadata_group_name + "Digital Flag"));
            LOG(logDEBUG) << "Digital Flag: " << m_digital_flag;
            if (m_digital_flag) {
                m_digital_samples = h5_get_scalar_dataset<int>(
                    file, std::string(metadata_group_name + "Digital Samples"));
            }
            LOG(logDEBUG) << "Digital Samples: " << m_digital_samples;
        } catch (H5::FileIException &e) {
            m_digital_flag = false;
        }

        // Dbit Offset
        try {
            m_dbit_offset = h5_get_scalar_dataset<int>(
                file, std::string(metadata_group_name + "Dbit Offset"));
            LOG(logDEBUG) << "Dbit Offset: " << m_dbit_offset;
        } catch (H5::FileIException &e) {
            // keep the optional empty
        }

        // Dbit List
        try {
            m_dbit_list = h5_get_scalar_dataset<size_t>(
                file, std::string(metadata_group_name + "Dbit Bitset List"));
            LOG(logDEBUG) << "Dbit list: " << m_dbit_list;
        } catch (H5::FileIException &e) {
            // keep the optional empty
        }

        // Transceiver Mask
        try {
            m_transceiver_mask = h5_get_scalar_dataset<int>(
                file, std::string(metadata_group_name + "Transceiver Mask"));
            LOG(logDEBUG) << "Transceiver Mask: " << m_transceiver_mask;
        } catch (H5::FileIException &e) {
            // keep the optional empty
        }

        // Transceiver Flag, Transceiver Samples
        try {
            m_transceiver_flag = h5_get_scalar_dataset<bool>(
                file, std::string(metadata_group_name + "Transceiver Flag"));
            LOG(logDEBUG) << "Transceiver Flag: " << m_transceiver_flag;
            if (m_transceiver_flag) {
                m_transceiver_samples = h5_get_scalar_dataset<int>(
                    file,
                    std::string(metadata_group_name + "Transceiver Samples"));
                LOG(logDEBUG)
                    << "Transceiver Samples: " << m_transceiver_samples;
            }
        } catch (H5::FileIException &e) {
            m_transceiver_flag = false;
        }

        // Rx ROI
        try {
            ROI tmp_roi;
            tmp_roi.xmin = h5_get_scalar_dataset<int>(
                file, std::string(metadata_group_name + "receiver roi xmin"));
            tmp_roi.xmax = h5_get_scalar_dataset<int>(
                file, std::string(metadata_group_name + "receiver roi xmax"));
            tmp_roi.ymin = h5_get_scalar_dataset<int>(
                file, std::string(metadata_group_name + "receiver roi ymin"));
            tmp_roi.ymax = h5_get_scalar_dataset<int>(
                file, std::string(metadata_group_name + "receiver roi ymax"));

            // if any of the values are set update the roi
            if (tmp_roi.xmin != 4294967295 || tmp_roi.xmax != 4294967295 ||
                tmp_roi.ymin != 4294967295 || tmp_roi.ymax != 4294967295) {
                // why?? TODO
                if (dVersion < 6.61) {
                    tmp_roi.xmax++;
                    tmp_roi.ymax++;
                }
                m_roi = tmp_roi;
            }
            // Not Done TODO
            // if we have an roi we need to update the geometry for the subfiles
            if (m_roi) {
            }
            LOG(logDEBUG) << "ROI: " << m_roi;
        } catch (H5::FileIException &e) {
            // keep the optional empty
        }

        // Update detector type for Moench
        // TODO! How does this work with old .h5 master files?
#ifdef AARE_VERBOSE
        fmt::print("Detecting Moench03: m_pixels_y: {}, "
                   "m_analog_samples: {}\n",
                   m_pixels_y, m_analog_samples.value_or(0));
#endif
        if (m_type == DetectorType::Moench && !m_analog_samples &&
            m_pixels_y == 400) {
            m_type = DetectorType::Moench03;
        } else if (m_type == DetectorType::Moench && m_pixels_y == 400 &&
                   m_analog_samples == 5000) {
            m_type = DetectorType::Moench03_old;
        }

        // Counter Mask
        try {
            m_counter_mask = h5_get_scalar_dataset<int>(
                file, std::string(metadata_group_name + "Counter Mask"));
            LOG(logDEBUG) << "Counter Mask: " << m_counter_mask;
        } catch (H5::FileIException &e) {
            // keep the optional empty
        }

        // Exposure Time Array
        try {
            m_exptime_array =
                StringTo<std::vector<ns>>(h5_get_scalar_dataset<std::string>(
                    file, std::string(metadata_group_name + "Exposure Times")));
            LOG(logDEBUG) << "Exposure Times: " << ToString(m_exptime_array);
        } catch (H5::FileIException &e) {
            // keep the optional empty
        }

        // Gate Delay Array
        try {
            m_gate_delay_array =
                StringTo<std::vector<ns>>(h5_get_scalar_dataset<std::string>(
                    file, std::string(metadata_group_name + "Gate Delays")));
            LOG(logDEBUG) << "Gate Delays: " << ToString(m_gate_delay_array);
        } catch (H5::FileIException &e) {
            // keep the optional empty
        }

        // Gates
        try {
            m_gates = h5_get_scalar_dataset<int>(
                file, std::string(metadata_group_name + "Gates"));
            LOG(logDEBUG) << "Gates: " << m_gates;
        } catch (H5::FileIException &e) {
            // keep the optional empty
        }

        // Additional Json Header
        try {
            m_additional_json_header =
                StringTo<std::map<std::string, std::string>>(
                    h5_get_scalar_dataset<std::string>(
                        file, std::string(metadata_group_name +
                                          "Additional JSON Header")));
            LOG(logDEBUG) << "Additional JSON Header: "
                          << ToString(m_additional_json_header);
        } catch (H5::FileIException &e) {
            // keep the optional empty
        }

        // Frames in File
        m_frames_in_file = h5_get_scalar_dataset<size_t>(
            file, std::string(metadata_group_name + "Frames in File"));
        LOG(logDEBUG) << "Frames in File: " << m_frames_in_file;

        H5Eset_auto(H5E_DEFAULT, reinterpret_cast<H5E_auto2_t>(H5Eprint2),
                    stderr);

    } catch (const H5::Exception &e) {
        fmt::print("Exception type: {}\n", typeid(e).name());
        e.printErrorStack();
        throw std::runtime_error(LOCATION + "\nCould not parse master file");
    }
}

} // namespace aare