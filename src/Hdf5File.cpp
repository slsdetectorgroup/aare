#include "aare/Hdf5File.hpp"
#include "aare/PixelMap.hpp"
#include "aare/defs.hpp"
#include "aare/logger.hpp"

#include <fmt/format.h>

namespace aare {

Hdf5File::Hdf5File(const std::filesystem::path &fname, const std::string &mode)
    : m_master(fname) {
    m_mode = mode;
    if (mode == "r") {
        open_data_file();
        open_header_files();
    } else {
        throw std::runtime_error(LOCATION +
                                 "Unsupported mode. Can only read Hdf5Files.");
    }
}

Frame Hdf5File::read_frame() { return get_frame(m_current_frame++); }

Frame Hdf5File::read_frame(size_t frame_number) {
    seek(frame_number);
    return read_frame();
}

std::vector<Frame> Hdf5File::read_n(size_t n_frames) {
    // TODO: implement this in a more efficient way
    std::vector<Frame> frames;
    for (size_t i = 0; i < n_frames; i++) {
        frames.push_back(this->get_frame(m_current_frame));
        m_current_frame++;
    }
    return frames;
}

void Hdf5File::read_into(std::byte *image_buf, size_t n_frames) {
    get_frame_into(m_current_frame++, image_buf, n_frames);
}

void Hdf5File::read_into(std::byte *image_buf) {
    get_frame_into(m_current_frame++, image_buf);
}

void Hdf5File::read_into(std::byte *image_buf, DetectorHeader *header) {

    get_frame_into(m_current_frame, image_buf, 1, header);
}

void Hdf5File::read_into(std::byte *image_buf, size_t n_frames,
                         DetectorHeader *header) {
    get_frame_into(m_current_frame++, image_buf, n_frames, header);
}

size_t Hdf5File::frame_number(size_t frame_index) {
    // TODO: check if it should check total_Frames() at any point
    //  check why this->read_into.. as in RawFile
    //  refactor multiple frame reads into a single one using hyperslab
    if (frame_index >= m_master.frames_in_file()) {
        throw std::runtime_error(LOCATION + " Frame number out of range");
    }
    uint64_t fnum{0};
    int part_index = 0; // assuming first part
    m_header_files[0]->get_header_into(frame_index, part_index,
                                       reinterpret_cast<std::byte *>(&fnum));
    return fnum;
}

size_t Hdf5File::bytes_per_frame() {
    return m_rows * m_cols * m_master.bitdepth() / 8;
}

size_t Hdf5File::pixels_per_frame() { return m_rows * m_cols; }
size_t Hdf5File::bytes_per_pixel() const { return m_master.bitdepth() / 8; }

void Hdf5File::seek(size_t frame_index) {
    m_data_file->seek(frame_index);
    for (size_t i = 0; i != header_dataset_names.size(); ++i) {
        m_header_files[i]->seek(frame_index);
    }
    m_current_frame = frame_index;
}

size_t Hdf5File::tell() { return m_current_frame; }
size_t Hdf5File::total_frames() const { return m_total_frames; }
size_t Hdf5File::rows() const { return m_rows; }
size_t Hdf5File::cols() const { return m_cols; }
size_t Hdf5File::bitdepth() const { return m_master.bitdepth(); }
xy Hdf5File::geometry() { return m_master.geometry(); }
size_t Hdf5File::n_modules() const { return m_master.n_modules(); }
Hdf5MasterFile Hdf5File::master() const { return m_master; }

DetectorType Hdf5File::detector_type() const {
    return m_master.detector_type();
}

Frame Hdf5File::get_frame(size_t frame_index) {
    auto f = Frame(m_rows, m_cols, Dtype::from_bitdepth(m_master.bitdepth()));
    std::byte *frame_buffer = f.data();
    get_frame_into(frame_index, frame_buffer);
    return f;
}

void Hdf5File::get_frame_into(size_t frame_index, std::byte *frame_buffer,
                              size_t n_frames, DetectorHeader *header) {
    if ((frame_index + n_frames - 1) >= m_master.frames_in_file()) {
        throw std::runtime_error(LOCATION + "Frame number out of range");
    }
    get_data_into(frame_index, frame_buffer);
    m_current_frame += n_frames;
    if (header) {
        for (size_t i = 0; i < n_frames; i++) {
            for (size_t part_idx = 0; part_idx != m_master.n_modules();
                 ++part_idx) {
                get_header_into(frame_index + i, part_idx, header);
                header++;
            }
        }
    }
}

void Hdf5File::get_data_into(size_t frame_index, std::byte *frame_buffer,
                             size_t n_frames) {
    m_data_file->get_data_into(frame_index, frame_buffer, n_frames);
}

void Hdf5File::get_header_into(size_t frame_index, int part_index,
                               DetectorHeader *header) {
    try {
        read_hdf5_header_fields(header,
                                [&](size_t iParameter, std::byte *dest) {
                                    m_header_files[iParameter]->get_header_into(
                                        frame_index, part_index, dest);
                                });
        LOG(logDEBUG5) << "Read 1D header for frame " << frame_index;
    } catch (const H5::Exception &e) {
        fmt::print("Exception type: {}\n", typeid(e).name());
        e.printErrorStack();
        throw std::runtime_error(
            LOCATION + "\nCould not to access header datasets in given file.");
    }
}

DetectorHeader Hdf5File::read_header(const std::filesystem::path &fname) {
    DetectorHeader h{};
    std::vector<std::unique_ptr<H5Handles>> handles;
    try {
        for (size_t i = 0; i != header_dataset_names.size(); ++i) {
            handles.push_back(std::make_unique<H5Handles>(
                fname.string(), metadata_group_name + header_dataset_names[i]));
        }
        read_hdf5_header_fields(&h, [&](size_t iParameter, std::byte *dest) {
            handles[iParameter]->get_header_into(0, 0, dest);
        });
        LOG(logDEBUG5) << "Read 1D header for frame 0";
    } catch (const H5::Exception &e) {
        handles.clear();
        fmt::print("Exception type: {}\n", typeid(e).name());
        e.printErrorStack();
        throw std::runtime_error(
            LOCATION + "\nCould not to access header datasets in given file.");
    }

    return h;
}

Hdf5File::~Hdf5File() {}

const std::string Hdf5File::metadata_group_name = "/entry/data/";
const std::vector<std::string> Hdf5File::header_dataset_names = {        
    "frame number",
    "exp length or sub exposure time",
    "packets caught",
    "detector specific 1",
    "timestamp",
    "mod id",
    "row",
    "column",
    "detector specific 2",
    "detector specific 3",
    "detector specific 4",
    "detector type",
    "detector header version",
    "packets caught bit mask"
};

void Hdf5File::open_data_file() {
    if (m_mode != "r")
        throw std::runtime_error(LOCATION +
                                 "Unsupported mode. Can only read Hdf5 files.");
    try {
        m_data_file = std::make_unique<H5Handles>(m_master.master_fname().string(), metadata_group_name + "/data");

        m_total_frames = m_data_file->get_dims()[0];
        m_rows = m_data_file->get_dims()[1];
        m_cols = m_data_file->get_dims()[2];
        //fmt::print("Data Dataset dimensions: frames = {}, rows = {}, cols = {}\n",
                  // m_total_frames, m_rows, m_cols);
    } catch (const H5::Exception &e) {
        m_data_file.reset();
        fmt::print("Exception type: {}\n", typeid(e).name());
        e.printErrorStack();
        throw std::runtime_error(
            LOCATION + "\nCould not to access 'data' dataset in master file.");
    }
}

void Hdf5File::open_header_files() {
    if (m_mode != "r")
        throw std::runtime_error(LOCATION +
                                 "Unsupported mode. Can only read Hdf5 files.");
    try {
        for (size_t i = 0; i != header_dataset_names.size(); ++i) {
            m_header_files.push_back(std::make_unique<H5Handles>(m_master.master_fname().string(), metadata_group_name + header_dataset_names[i]));
            //fmt::print("{} Dataset dimensions: size = {}\n",
             //      header_dataset_names[i], m_header_files[i]->dims[0]);
        }
    } catch (const H5::Exception &e) {
        m_header_files.clear();
        m_data_file.reset();
        fmt::print("Exception type: {}\n", typeid(e).name());
        e.printErrorStack();
        throw std::runtime_error(
            LOCATION + "\nCould not to access header datasets in master file.");
    }
}

} // namespace aare