#include "aare/Hdf5MasterFile.hpp"
#include <sstream>
namespace aare {

Hdf5FileNameComponents::Hdf5FileNameComponents(
    const std::filesystem::path &fname) {
    m_base_path = fname.parent_path();
    m_base_name = fname.stem();
    m_ext = fname.extension();

    if (m_ext != ".h5") {
        throw std::runtime_error(LOCATION +
                                 "Unsupported file type. (only .h5)");
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

std::filesystem::path Hdf5FileNameComponents::master_fname() const {
    return m_base_path /
           fmt::format("{}_master_{}{}", m_base_name, m_file_index, m_ext);
}

std::filesystem::path Hdf5FileNameComponents::data_fname(size_t mod_id,
                                                         size_t file_id) const {

    std::string fmt = "{}_d{}_f{}_{}.h5";
    // Before version X we used to name the data files f000000000000
    if (m_old_scheme) {
        fmt = "{}_d{}_f{:012}_{}.h5";
    }
    return m_base_path /
           fmt::format(fmt, m_base_name, mod_id, file_id, m_file_index);
}

void Hdf5FileNameComponents::set_old_scheme(bool old_scheme) {
    m_old_scheme = old_scheme;
}

const std::filesystem::path &Hdf5FileNameComponents::base_path() const {
    return m_base_path;
}
const std::string &Hdf5FileNameComponents::base_name() const {
    return m_base_name;
}
const std::string &Hdf5FileNameComponents::ext() const { return m_ext; }
int Hdf5FileNameComponents::file_index() const { return m_file_index; }

// "[enabled\ndac dac 4\nstart 500\nstop 2200\nstep 5\nsettleTime 100us\n]"
/*ScanParameters::ScanParameters(const std::string &par) {
    std::istringstream iss(par.substr(1, par.size() - 2));
    std::string line;
    while (std::getline(iss, line)) {
        if (line == "enabled") {
            m_enabled = true;
        } else if (line.find("dac") != std::string::npos) {
            m_dac = line.substr(4);
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
void ScanParameters::increment_stop() { m_stop += 1; };
//int ScanParameters::step() const { return m_step; }
const std::string &ScanParameters::dac() const { return m_dac; }
bool ScanParameters::enabled() const { return m_enabled; }
*/
Hdf5MasterFile::Hdf5MasterFile(const std::filesystem::path &fpath)
    : m_fnc(fpath) {
    if (!std::filesystem::exists(fpath)) {
        throw std::runtime_error(LOCATION + " File does not exist");
    }
    if (m_fnc.ext() == ".h5") {
        parse_acquisition_metadata(fpath);
    } else {
        throw std::runtime_error(LOCATION + "Unsupported file type");
    }
}

std::filesystem::path Hdf5MasterFile::master_fname() const {
    return m_fnc.master_fname();
}

std::filesystem::path Hdf5MasterFile::data_fname(size_t mod_id,
                                                 size_t file_id) const {
    return m_fnc.data_fname(mod_id, file_id);
}

const std::string &Hdf5MasterFile::version() const { return m_version; }
const DetectorType &Hdf5MasterFile::detector_type() const { return m_type; }
const TimingMode &Hdf5MasterFile::timing_mode() const { return m_timing_mode; }
size_t Hdf5MasterFile::image_size_in_bytes() const {
    return m_image_size_in_bytes;
}
size_t Hdf5MasterFile::frames_in_file() const { return m_frames_in_file; }
size_t Hdf5MasterFile::pixels_y() const { return m_pixels_y; }
size_t Hdf5MasterFile::pixels_x() const { return m_pixels_x; }
size_t Hdf5MasterFile::max_frames_per_file() const {
    return m_max_frames_per_file;
}
size_t Hdf5MasterFile::bitdepth() const { return m_bitdepth; }
size_t Hdf5MasterFile::frame_padding() const { return m_frame_padding; }
const FrameDiscardPolicy &Hdf5MasterFile::frame_discard_policy() const {
    return m_frame_discard_policy;
}

size_t Hdf5MasterFile::total_frames_expected() const {
    return m_total_frames_expected;
}

std::optional<size_t> Hdf5MasterFile::number_of_rows() const {
    return m_number_of_rows;
}

xy Hdf5MasterFile::geometry() const { return m_geometry; }

std::optional<uint8_t> Hdf5MasterFile::quad() const { return m_quad; }

// optional values, these may or may not be present in the master file
// and are therefore modeled as std::optional
std::optional<size_t> Hdf5MasterFile::analog_samples() const {
    return m_analog_samples;
}
std::optional<size_t> Hdf5MasterFile::digital_samples() const {
    return m_digital_samples;
}

std::optional<size_t> Hdf5MasterFile::transceiver_samples() const {
    return m_transceiver_samples;
}

/*
ScanParameters Hdf5MasterFile::scan_parameters() const {
    return m_scan_parameters;
}
*/

// std::optional<ROI> Hdf5MasterFile::roi() const { return m_roi; }

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
    char buffer[257]{0};
    dataset.read(buffer, data_type);
    return std::string(buffer);
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
        {
            H5::Attribute attr = file.openAttribute("version");
            H5::DataType attr_type = attr.getDataType();
            double value{0.0};
            attr.read(attr_type, &value);
            std::ostringstream oss;
            oss << std::fixed << std::setprecision(1) << value;
            m_version = oss.str();
            // fmt::print("Version: {}\n", m_version);
        }

        // Scalar Dataset
        // Detector Type
        m_type = StringTo<DetectorType>(h5_get_scalar_dataset<std::string>(
            file, std::string(metadata_group_name + "Detector Type")));
        // fmt::print("Detector Type: {}\n", (ToString(m_type)));

        // Timing Mode
        m_timing_mode = StringTo<TimingMode>(h5_get_scalar_dataset<std::string>(
            file, std::string(metadata_group_name + "Timing Mode")));
        // fmt::print("Timing Mode: {}\n", (ToString(m_timing_mode)));

        // Geometry
        m_geometry.row = h5_get_scalar_dataset<int>(
            file, std::string(metadata_group_name + "Geometry in y axis"));
        m_geometry.col = h5_get_scalar_dataset<int>(
            file, std::string(metadata_group_name + "Geometry in x axis"));
        // fmt::print("Geometry: {}\n", m_geometry.to_string());

        // Image Size
        m_image_size_in_bytes = h5_get_scalar_dataset<int>(
            file, std::string(metadata_group_name + "Image Size"));
        // fmt::print("Image size: {}\n", m_image_size_in_bytes);

        // Frames in File
        m_frames_in_file = h5_get_scalar_dataset<uint64_t>(
            file, std::string(metadata_group_name + "Frames in File"));
        // fmt::print("Frames in File: {}\n", m_frames_in_file);

        // Pixels
        m_pixels_y = h5_get_scalar_dataset<int>(
            file,
            std::string(metadata_group_name + "Number of pixels in y axis"));
        // fmt::print("Pixels in y: {}\n", m_pixels_y);
        m_pixels_x = h5_get_scalar_dataset<int>(
            file,
            std::string(metadata_group_name + "Number of pixels in x axis"));
        // fmt::print("Pixels in x: {}\n", m_pixels_x);

        // Max Frames per File
        m_max_frames_per_file = h5_get_scalar_dataset<int>(
            file, std::string(metadata_group_name + "Maximum frames per file"));
        // fmt::print("Max frames per File: {}\n", m_max_frames_per_file);

        // Bit Depth
        // Not all detectors write the bitdepth but in case
        // its not there it is 16
        H5::Exception::dontPrint();
        try {
            m_bitdepth = h5_get_scalar_dataset<int>(
                file, std::string(metadata_group_name + "Dynamic Range"));
        } catch (H5::FileIException &e) {
            m_bitdepth = 16;
        }
        // fmt::print("Bit Depth: {}\n", m_bitdepth);
        H5Eset_auto(H5E_DEFAULT, reinterpret_cast<H5E_auto2_t>(H5Eprint2),
                    stderr);

        // Total Frames
        m_total_frames_expected = h5_get_scalar_dataset<uint64_t>(
            file, std::string(metadata_group_name + "Total Frames"));
        // fmt::print("Total Frames: {}\n", m_total_frames_expected);

        // Frame Padding
        m_frame_padding = h5_get_scalar_dataset<int>(
            file, std::string(metadata_group_name + "Frame Padding"));
        // fmt::print("Frame Padding: {}\n", m_frame_padding);

        // Frame Discard Policy
        m_frame_discard_policy =
            StringTo<FrameDiscardPolicy>(h5_get_scalar_dataset<std::string>(
                file,
                std::string(metadata_group_name + "Frame Discard Policy")));
        // fmt::print("Frame Discard Policy: {}\n",
        // (ToString(m_frame_discard_policy)));

        // Number of rows
        // Not all detectors write the Number of rows but in case
        H5::Exception::dontPrint();
        try {
            m_number_of_rows = h5_get_scalar_dataset<int>(
                file, std::string(metadata_group_name + "Number of rows"));
        } catch (H5::FileIException &e) {
            // keep the optional empty
        }
        // fmt::print("Number of rows: {}\n", m_number_of_rows);
        H5Eset_auto(H5E_DEFAULT, reinterpret_cast<H5E_auto2_t>(H5Eprint2),
                    stderr);

        // Analog Flag
        // ----------------------------------------------------------------
        // Special treatment of analog flag because of Moench03
        H5::Exception::dontPrint();
        try {
            m_analog_flag = h5_get_scalar_dataset<int>(
                file, std::string(metadata_group_name + "Analog Flag"));
        } catch (H5::FileIException &e) {
            // if it doesn't work still set it to one
            // to try to decode analog samples (Old Moench03)
            m_analog_flag = 1;
        }
        // fmt::print("Analog Flag: {}\n", m_analog_flag);
        H5Eset_auto(H5E_DEFAULT, reinterpret_cast<H5E_auto2_t>(H5Eprint2),
                    stderr);

        // Analog Samples
        H5::Exception::dontPrint();
        try {
            if (m_analog_flag) {
                m_analog_samples = h5_get_scalar_dataset<int>(
                    file, std::string(metadata_group_name + "Analog Samples"));
            }
        } catch (H5::FileIException &e) {
            // keep the optional empty
            // and set analog flag to 0
            m_analog_flag = 0;
        }
        H5Eset_auto(H5E_DEFAULT, reinterpret_cast<H5E_auto2_t>(H5Eprint2),
                    stderr);
        // fmt::print("Analog Samples: {}\n", m_analog_samples);
        //-----------------------------------------------------------------

        // Quad
        H5::Exception::dontPrint();
        try {
            m_quad = h5_get_scalar_dataset<int>(
                file, std::string(metadata_group_name + "Quad"));
        } catch (H5::FileIException &e) {
            // keep the optional empty
        }
        // fmt::print("Quad: {}\n", m_quad);
        H5Eset_auto(H5E_DEFAULT, reinterpret_cast<H5E_auto2_t>(H5Eprint2),
                    stderr);

        // ADC Mask
        // H5::Exception::dontPrint();
        // try {
        //     m_adc_mask = h5_get_scalar_dataset<int>(
        //         file, std::string(metadata_group_name + "ADC
        //         Mask"));
        // } catch (H5::FileIException &e) {
        //     m_adc_mask = 0;
        // }
        // fmt::print("ADC Mask: {}\n", m_adc_mask);
        // H5Eset_auto(H5E_DEFAULT,
        // reinterpret_cast<H5E_auto2_t>(H5Eprint2),
        //             stderr);

        // Digital Flag, Digital Samples
        H5::Exception::dontPrint();
        try {
            m_digital_flag = h5_get_scalar_dataset<int>(
                file, std::string(metadata_group_name + "Digital Flag"));
            if (m_digital_flag) {
                m_digital_samples = h5_get_scalar_dataset<int>(
                    file, std::string(metadata_group_name + "Digital Samples"));
            }
        } catch (H5::FileIException &e) {
            // keep the optional empty
        }
        // fmt::print("Digital Flag: {}\n", m_digital_flag);
        // fmt::print("Digital Samples: {}\n", m_digital_samples);
        H5Eset_auto(H5E_DEFAULT, reinterpret_cast<H5E_auto2_t>(H5Eprint2),
                    stderr);

        // Transceiver Flag, Transceiver Samples
        H5::Exception::dontPrint();
        try {
            m_transceiver_flag = h5_get_scalar_dataset<int>(
                file, std::string(metadata_group_name + "Transceiver Flag"));
            if (m_transceiver_flag) {
                m_transceiver_samples = h5_get_scalar_dataset<int>(
                    file,
                    std::string(metadata_group_name + "Transceiver Samples"));
            }
        } catch (H5::FileIException &e) {
            // keep the optional empty
        }
        // fmt::print("Transceiver Flag: {}\n", m_transceiver_flag);
        // fmt::print("Transceiver Samples: {}\n",
        // m_transceiver_samples);
        H5Eset_auto(H5E_DEFAULT, reinterpret_cast<H5E_auto2_t>(H5Eprint2),
                    stderr);

        // scan parameters
        /*try{
            std::string scan_parameters = j.at("Scan Parameters");
            m_scan_parameters = ScanParameters(scan_parameters);
            if(v<7.21){
                    m_scan_parameters.increment_stop(); //adjust for
    endpoint being included
                }
        }catch (const json::out_of_range &e) {
            // not a scan
        }

    try{
        ROI tmp_roi;
        auto obj = j.at("Receiver Roi");
        tmp_roi.xmin = obj.at("xmin");
        tmp_roi.xmax = obj.at("xmax");
        tmp_roi.ymin = obj.at("ymin");
        tmp_roi.ymax = obj.at("ymax");

        //if any of the values are set update the roi
        if (tmp_roi.xmin != 4294967295 || tmp_roi.xmax != 4294967295
    || tmp_roi.ymin != 4294967295 || tmp_roi.ymax != 4294967295) {

            if(v<7.21){
                tmp_roi.xmax++;
                tmp_roi.ymax++;
            }

            m_roi = tmp_roi;
        }


    }catch (const json::out_of_range &e) {
        // leave the optional empty
    }
           //if we have an roi we need to update the geometry for the
    subfiles if (m_roi){

    }
        */

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

        file.close();

    } catch (const H5::Exception &e) {
        fmt::print("Exception type: {}\n", typeid(e).name());
        e.printErrorStack();
        throw std::runtime_error(LOCATION + "\nCould not parse master file");
    }
}

} // namespace aare