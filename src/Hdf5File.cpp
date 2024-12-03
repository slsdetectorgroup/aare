#include "aare/Hdf5File.hpp"
#include "aare/PixelMap.hpp"
#include "aare/defs.hpp"

#include <fmt/format.h>

namespace aare {

Hdf5File::Hdf5File(const std::filesystem::path &fname, const std::string &mode)
    : m_master(fname) {
    m_mode = mode;
    if (mode == "r") {
        open_file();
    } else {
        throw std::runtime_error(LOCATION +
                                 "Unsupported mode. Can only read Hdf5Files.");
    }
}

Frame Hdf5File::read_frame() { return get_frame(m_current_frame++); };

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
};

void Hdf5File::read_into(std::byte *image_buf, DetectorHeader *header) {

    return get_frame_into(m_current_frame++, image_buf, header);
};

void Hdf5File::read_into(std::byte *image_buf, size_t n_frames,
                         DetectorHeader *header) {
    // return get_frame_into(m_current_frame++, image_buf, header);

    for (size_t i = 0; i < n_frames; i++) {
        this->get_frame_into(m_current_frame++, image_buf, header);
        image_buf += bytes_per_frame();
        if (header)
            header += n_mod();
    }
};

size_t Hdf5File::n_mod() const { return 1; } // n_subfile_parts; }

size_t Hdf5File::bytes_per_frame() {
    return m_rows * m_cols * m_master.bitdepth() / 8;
}
size_t Hdf5File::pixels_per_frame() { return m_rows * m_cols; }

DetectorType Hdf5File::detector_type() const {
    return m_master.detector_type();
}

void Hdf5File::seek(size_t frame_index) {
    // check if the frame number is greater than the total frames
    // if frame_number == total_frames, then the next read will throw an error
    if (frame_index > total_frames()) {
        throw std::runtime_error(
            fmt::format("frame number {} is greater than total frames {}",
                        frame_index, total_frames()));
    }
    m_current_frame = frame_index;
};

size_t Hdf5File::tell() { return m_current_frame; };

size_t Hdf5File::total_frames() const { return m_total_frames; }
size_t Hdf5File::rows() const { return m_rows; }
size_t Hdf5File::cols() const { return m_cols; }
size_t Hdf5File::bitdepth() const { return m_master.bitdepth(); }
xy Hdf5File::geometry() { return m_master.geometry(); }

DetectorHeader Hdf5File::read_header(const std::filesystem::path &fname) {
    DetectorHeader h{};
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

    // Check if the frame number is valid
    if (frame_index < 0 || frame_index >= m_total_frames) {
        throw std::runtime_error(LOCATION + "Invalid frame number");
    }

    // Define the hyperslab to select the 2D slice for the given frame number
    hsize_t offset[3] = {static_cast<hsize_t>(frame_index), 0, 0};
    hsize_t count[3] = {1, m_rows, m_cols};
    dataspace->selectHyperslab(H5S_SELECT_SET, count, offset);

    // Define the memory space for the 2D slice
    hsize_t dimsm[2] = {m_rows, m_cols};
    H5::DataSpace memspace(2, dimsm);

    // Read the data into the provided 2D array
    dataset->read(frame_buffer, m_datatype, memspace, dataspace);

    fmt::print("Read 2D data for frame {}\n", frame_index);
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
    if (frame_index >= m_master.frames_in_file()) {
        throw std::runtime_error(LOCATION + " Frame number out of range");
    }
    size_t subfile_id = frame_index / m_master.max_frames_per_file();
    /*if (subfile_id >= subfiles.size()) {
        throw std::runtime_error(
            LOCATION + " Subfile out of range. Possible missing data.");
    }*/
    return 1; // subfiles[subfile_id][0]->frame_number(
              // frame_index % m_master.max_frames_per_file());
}

Hdf5File::~Hdf5File() {}

const std::string Hdf5File::metadata_group_name = "/entry/data/";

void Hdf5File::open_file() {
    if (m_mode != "r")
        throw std::runtime_error(LOCATION +
                                 "Unsupported mode. Can only read Hdf5 files.");
    try {
        file = std::make_unique<H5::H5File>(m_master.master_fname().string(),
                                            H5F_ACC_RDONLY);
        dataset = std::make_unique<H5::DataSet>(
            file->openDataSet(metadata_group_name + "/data"));
        dataspace = std::make_unique<H5::DataSpace>(dataset->getSpace());
        int rank = dataspace->getSimpleExtentNdims();
        if (rank != 3) {
            throw std::runtime_error(
                LOCATION + "Expected rank of '/data' dataset to be 3. Got " +
                std::to_string(rank));
        }
        hsize_t dims[3];
        dataspace->getSimpleExtentDims(dims, nullptr);
        m_total_frames = dims[0];
        m_rows = dims[1];
        m_cols = dims[2];
        m_datatype = dataset->getDataType();
        fmt::print("Dataset dimensions: frames = {}, rows = {}, cols = {}\n",
                   m_total_frames, m_rows, m_cols);
    } catch (const H5::Exception &e) {
        file->close();
        fmt::print("Exception type: {}\n", typeid(e).name());
        e.printErrorStack();
        throw std::runtime_error(
            LOCATION + "\nCould not to access 'data' dataset in master file.");
    }
}

} // namespace aare