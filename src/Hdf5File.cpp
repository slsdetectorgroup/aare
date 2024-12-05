#include "aare/Hdf5File.hpp"
#include "aare/PixelMap.hpp"
#include "aare/defs.hpp"

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

void Hdf5File::read_into(std::byte *image_buf, size_t n_frames) {
    // TODO: implement this in a more efficient way

    for (size_t i = 0; i < n_frames; i++) {
        this->get_frame_into(m_current_frame++, image_buf);
        image_buf += bytes_per_frame();
    }
}

void Hdf5File::read_into(std::byte *image_buf) {
    return get_frame_into(m_current_frame++, image_buf);
}

void Hdf5File::read_into(std::byte *image_buf, DetectorHeader *header) {

    return get_frame_into(m_current_frame++, image_buf, header);
}

void Hdf5File::read_into(std::byte *image_buf, size_t n_frames,
                         DetectorHeader *header) {
    // return get_frame_into(m_current_frame++, image_buf, header);

    for (size_t i = 0; i < n_frames; i++) {
        this->get_frame_into(m_current_frame++, image_buf, header);
        image_buf += bytes_per_frame();
        if (header)
            header += n_mod();
    }
}

size_t Hdf5File::n_mod() const { return 1; }

size_t Hdf5File::bytes_per_frame() {
    return m_rows * m_cols * m_master.bitdepth() / 8;
}
size_t Hdf5File::pixels_per_frame() { return m_rows * m_cols; }

DetectorType Hdf5File::detector_type() const {
    return m_master.detector_type();
}

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

DetectorHeader Hdf5File::read_header(const std::filesystem::path &fname) {
    DetectorHeader h{};
    std::vector<std::unique_ptr<H5Handles>> handles;
    try {
        for (size_t i = 0; i != header_dataset_names.size(); ++i) {
            handles.push_back(std::make_unique<H5Handles>(fname.string(), metadata_group_name+ header_dataset_names[i]));
        }
        handles[0]->get_frame_into(0, reinterpret_cast<std::byte *>(&(h.frameNumber)));
        handles[1]->get_frame_into(0, reinterpret_cast<std::byte *>(&(h.expLength)));
        handles[2]->get_frame_into(0, reinterpret_cast<std::byte *>(&(h.packetNumber)));
        handles[3]->get_frame_into(0, reinterpret_cast<std::byte *>(&(h.bunchId)));
        handles[4]->get_frame_into(0, reinterpret_cast<std::byte *>(&(h.timestamp)));
        handles[5]->get_frame_into(0, reinterpret_cast<std::byte *>(&(h.modId)));
        handles[6]->get_frame_into(0, reinterpret_cast<std::byte *>(&(h.row)));
        handles[7]->get_frame_into(0, reinterpret_cast<std::byte *>(&(h.column)));
        handles[8]->get_frame_into(0, reinterpret_cast<std::byte *>(&(h.reserved)));
        handles[9]->get_frame_into(0, reinterpret_cast<std::byte *>(&(h.debug)));
        handles[10]->get_frame_into(0, reinterpret_cast<std::byte *>(&(h.roundRNumber)));
        handles[11]->get_frame_into(0, reinterpret_cast<std::byte *>(&(h.detType)));
        handles[12]->get_frame_into(0, reinterpret_cast<std::byte *>(&(h.version)));
        handles[13]->get_frame_into(0, reinterpret_cast<std::byte *>(&(h.packetMask)));
        
        fmt::print("Read 1D header for frame {}\n", 0);
    } catch (const H5::Exception &e) {
        handles.clear();
        fmt::print("Exception type: {}\n", typeid(e).name());
        e.printErrorStack();
        throw std::runtime_error(
            LOCATION + "\nCould not to access header datasets in given file.");
    }



    FILE *fp = fopen(fname.string().c_str(), "r");
    if (!fp)
        throw std::runtime_error(
            fmt::format("Could not open: {} for reading", fname.string()));

    size_t const rc = fread(reinterpret_cast<char *>(&h), sizeof(h), 1, fp);
    if (rc != 1)
        throw std::runtime_error(LOCATION + "Could not read header from file");
    if (fclose(fp)) {
        throw std::runtime_error(LOCATION + "Could not close file");
    }

    return h;
}

Hdf5MasterFile Hdf5File::master() const { return m_master; }

Frame Hdf5File::get_frame(size_t frame_index) {
    auto f = Frame(m_rows, m_cols, Dtype::from_bitdepth(m_master.bitdepth()));
    std::byte *frame_buffer = f.data();
    get_frame_into(frame_index, frame_buffer);
    return f;
}

size_t Hdf5File::bytes_per_pixel() const { return m_master.bitdepth() / 8; }

void Hdf5File::get_frame_into(size_t frame_index, std::byte *frame_buffer,
                              DetectorHeader *header) {
    get_data_into(frame_index, frame_buffer);
    get_header_into(frame_index, header);
}

void Hdf5File::get_data_into(size_t frame_index, std::byte *frame_buffer) {
    m_data_file->get_frame_into(frame_index, frame_buffer);
    //fmt::print("Read 2D data for frame {}\n", frame_index);
}

void Hdf5File::get_header_into(size_t frame_index, DetectorHeader *header) {
    if (header) {
        m_header_files[0]->get_frame_into(frame_index, reinterpret_cast<std::byte *>(&(header->frameNumber)));
        m_header_files[1]->get_frame_into(frame_index, reinterpret_cast<std::byte *>(&(header->expLength)));
        m_header_files[2]->get_frame_into(frame_index, reinterpret_cast<std::byte *>(&(header->packetNumber)));
        m_header_files[3]->get_frame_into(frame_index, reinterpret_cast<std::byte *>(&(header->bunchId)));
        m_header_files[4]->get_frame_into(frame_index, reinterpret_cast<std::byte *>(&(header->timestamp)));
        m_header_files[5]->get_frame_into(frame_index, reinterpret_cast<std::byte *>(&(header->modId)));
        m_header_files[6]->get_frame_into(frame_index, reinterpret_cast<std::byte *>(&(header->row)));
        m_header_files[7]->get_frame_into(frame_index, reinterpret_cast<std::byte *>(&(header->column)));
        m_header_files[8]->get_frame_into(frame_index, reinterpret_cast<std::byte *>(&(header->reserved)));
        m_header_files[9]->get_frame_into(frame_index, reinterpret_cast<std::byte *>(&(header->debug)));
        m_header_files[10]->get_frame_into(frame_index, reinterpret_cast<std::byte *>(&(header->roundRNumber)));
        m_header_files[11]->get_frame_into(frame_index, reinterpret_cast<std::byte *>(&(header->detType)));
        m_header_files[12]->get_frame_into(frame_index, reinterpret_cast<std::byte *>(&(header->version)));
        m_header_files[13]->get_frame_into(frame_index, reinterpret_cast<std::byte *>(&(header->packetMask)));
        fmt::print("Read 1D header for frame {}\n", frame_index);
    }
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

size_t Hdf5File::frame_number(size_t frame_index) {
    uint64_t fnum{0};
    m_header_files[0]->get_frame_into(frame_index, reinterpret_cast<std::byte *>(&fnum));
    return fnum;
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

        m_total_frames = m_data_file->dims[0];
        m_rows = m_data_file->dims[1];
        m_cols = m_data_file->dims[2];
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