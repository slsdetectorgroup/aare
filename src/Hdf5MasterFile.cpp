#include "aare/Hdf5MasterFile.hpp"
#include "aare/logger.hpp"
#include <sstream>
#include <iomanip>
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
xy Hdf5MasterFile::geometry() const { return m_geometry; }
size_t Hdf5MasterFile::image_size_in_bytes() const {
    return m_image_size_in_bytes;
}
size_t Hdf5MasterFile::pixels_y() const { return m_pixels_y; }
size_t Hdf5MasterFile::pixels_x() const { return m_pixels_x; }
size_t Hdf5MasterFile::max_frames_per_file() const {
    return m_max_frames_per_file;
}
const FrameDiscardPolicy &Hdf5MasterFile::frame_discard_policy() const {
    return m_frame_discard_policy;
}
size_t Hdf5MasterFile::frame_padding() const { return m_frame_padding; }
ScanParameters Hdf5MasterFile::scan_parameters() const {
    return m_scan_parameters;
}
size_t Hdf5MasterFile::total_frames_expected() const {
    return m_total_frames_expected;
}
// exptime
// period
// burst mode
// num udp interfaces 
size_t Hdf5MasterFile::bitdepth() const { return m_bitdepth; }
// ten giga
// thresholdenergy
// thresholdall energy
// subexptime
// subperiod
std::optional<uint8_t> Hdf5MasterFile::quad() const { return m_quad; }
std::optional<size_t> Hdf5MasterFile::number_of_rows() const {
    return m_number_of_rows;
}
std::optional<uint32_t> Hdf5MasterFile::adc_mask() const { return m_adc_mask;}
std::optional<uint8_t> Hdf5MasterFile::analog_flag() const { return m_analog_flag; }  
std::optional<size_t> Hdf5MasterFile::analog_samples() const {
    return m_analog_samples;
}
std::optional<uint8_t> Hdf5MasterFile::digital_flag() const { return m_digital_flag; }
std::optional<size_t> Hdf5MasterFile::digital_samples() const {
    return m_digital_samples;
}
// dbitoffset
// dbitlist
// transceiver mask  
std::optional<uint8_t> Hdf5MasterFile::transceiver_flag() const { return m_transceiver_flag; }
std::optional<size_t> Hdf5MasterFile::transceiver_samples() const {
    return m_transceiver_samples;
}
// g1 roi
std::optional<ROI> Hdf5MasterFile::roi() const { return m_roi; }
// counter mask
// exptimearray
// gatedelay array
// gates
// additional json header
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
        LOG(logDEBUG) << "Image size: {}\n" << m_image_size_in_bytes;

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

        // Image Size in Bytes
        m_max_frames_per_file = h5_get_scalar_dataset<int>(
            file, std::string(metadata_group_name + "Maximum frames per file"));
        LOG(logDEBUG) << "Max frames per File: " << m_max_frames_per_file;

        // Frame Discard Policy
        m_frame_discard_policy =
            StringTo<FrameDiscardPolicy>(h5_get_scalar_dataset<std::string>(
                file,
                std::string(metadata_group_name + "Frame Discard Policy")));
        LOG(logDEBUG) << "Frame Discard Policy: " << ToString(m_frame_discard_policy);

        // Frame Padding
        m_frame_padding = h5_get_scalar_dataset<int>(
            file, std::string(metadata_group_name + "Frame Padding"));
        LOG(logDEBUG) << "Frame Padding: " << m_frame_padding;


        // Scan Parameters
        try {
            std::string scan_parameters = h5_get_scalar_dataset<std::string>(
            file, std::string(metadata_group_name + "Scan Parameters"));
            m_scan_parameters = ScanParameters(scan_parameters);
            if (dVersion < 6.61){
                m_scan_parameters.increment_stop(); //adjust for endpoint being included
            }
        } catch (H5::FileIException &e) {
            // keep the optional empty
        }
        LOG(logDEBUG) << "Scan Parameters: " << ToString(m_scan_parameters);

        // Total Frames Expected
        m_total_frames_expected = h5_get_scalar_dataset<uint64_t>(
            file, std::string(metadata_group_name + "Total Frames"));
        LOG(logDEBUG) << "Total Frames: " << m_total_frames_expected;

        // exptime
        // period
        // burst mode
        // num udp interfaces 

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
        LOG(logDEBUG) << "Bit Depth: " << m_bitdepth;
        H5Eset_auto(H5E_DEFAULT, reinterpret_cast<H5E_auto2_t>(H5Eprint2), stderr);

        // ten giga
        // thresholdenergy
        // thresholdall energy
        // subexptime
        // subperiod

        // Quad
        H5::Exception::dontPrint();
        try {
            m_quad = h5_get_scalar_dataset<int>(
                file, std::string(metadata_group_name + "Quad"));
        } catch (H5::FileIException &e) {
            // keep the optional empty
        }
        LOG(logDEBUG) << "Quad: " << m_quad;
        H5Eset_auto(H5E_DEFAULT, reinterpret_cast<H5E_auto2_t>(H5Eprint2),
                    stderr);

        // Number of Rows
        // Not all detectors write the Number of rows but in case
        H5::Exception::dontPrint();
        try {
            m_number_of_rows = h5_get_scalar_dataset<int>(
                file, std::string(metadata_group_name + "Number of rows"));
        } catch (H5::FileIException &e) {
            // keep the optional empty
        }
        LOG(logDEBUG) << "Number of rows: " << m_number_of_rows;
        H5Eset_auto(H5E_DEFAULT, reinterpret_cast<H5E_auto2_t>(H5Eprint2),
                    stderr);

        // ratecorr
    
        // ADC Mask
        H5::Exception::dontPrint();
        try {
            m_adc_mask = h5_get_scalar_dataset<int>(
                file, std::string(metadata_group_name + "ADC Mask"));
        } catch (H5::FileIException &e) {
            m_adc_mask = 0;
        }
        LOG(logDEBUG) << "ADC Mask: " << m_adc_mask;
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
        LOG(logDEBUG) << "Analog Flag: " << m_analog_flag;
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
        LOG(logDEBUG) << "Analog Samples: " << m_analog_samples;
        //-----------------------------------------------------------------

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
        LOG(logDEBUG) << "Digital Flag: " << m_digital_flag;
        LOG(logDEBUG) << "Digital Samples: " << m_digital_samples;
        H5Eset_auto(H5E_DEFAULT, reinterpret_cast<H5E_auto2_t>(H5Eprint2),
                    stderr);

        // dbitoffset
        // dbitlist
        // transceiver mask  

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
        LOG(logDEBUG) << "Transceiver Flag: " << m_transceiver_flag;
        LOG(logDEBUG) << "Transceiver Samples: " << m_transceiver_samples;
        H5Eset_auto(H5E_DEFAULT, reinterpret_cast<H5E_auto2_t>(H5Eprint2),
                    stderr);

        // ROI
        try {
            std::string scan_parameters = h5_get_scalar_dataset<std::string>(
            file, std::string(metadata_group_name + "Scan Parameters"));
            m_scan_parameters = ScanParameters(scan_parameters);
            if (dVersion < 6.61){
                m_scan_parameters.increment_stop(); //adjust for endpoint being included
            }
        } catch (H5::FileIException &e) {
            // keep the optional empty
        }
        LOG(logDEBUG) << "Scan Parameters: " << ToString(m_scan_parameters);

        try{
            ROI tmp_roi;
            tmp_roi.xmin = h5_get_scalar_dataset<int>(
                file, std::string(metadata_group_name + "receiver roi xmin"));
            tmp_roi.xmax = h5_get_scalar_dataset<int>(
                file, std::string(metadata_group_name + "receiver roi xmax"));
            tmp_roi.ymin = h5_get_scalar_dataset<int>(
                file, std::string(metadata_group_name + "receiver roi ymin"));
            tmp_roi.ymax = h5_get_scalar_dataset<int>(
                file, std::string(metadata_group_name + "receiver roi ymax"));

            //if any of the values are set update the roi
            if (tmp_roi.xmin != 4294967295 || tmp_roi.xmax != 4294967295 || tmp_roi.ymin != 4294967295 || tmp_roi.ymax != 4294967295) {
                //why?? TODO
                if(dVersion < 6.61){
                    tmp_roi.xmax++;
                    tmp_roi.ymax++;
                }
                m_roi = tmp_roi;
            }
        } catch (H5::FileIException &e) {
            // keep the optional empty
        }

        // Not Done TODO
        //if we have an roi we need to update the geometry for the subfiles
        if (m_roi){

        }
        LOG(logDEBUG) << "ROI: " << m_roi;


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

        // counter mask
        // exptimearray
        // gatedelay array
        // gates
        // additional json header

        // Frames in File
        m_frames_in_file = h5_get_scalar_dataset<uint64_t>(
            file, std::string(metadata_group_name + "Frames in File"));
        LOG(logDEBUG) << "Frames in File: " << m_frames_in_file;


    } catch (const H5::Exception &e) {
        fmt::print("Exception type: {}\n", typeid(e).name());
        e.printErrorStack();
        throw std::runtime_error(LOCATION + "\nCould not parse master file");
    }
}

} // namespace aare