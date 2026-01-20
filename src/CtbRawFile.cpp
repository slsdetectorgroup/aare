#include "aare/CtbRawFile.hpp"

#include <fmt/format.h>
namespace aare {

CtbRawFile::CtbRawFile(const std::filesystem::path &fname) : m_master(fname) {
    if (m_master.detector_type() != DetectorType::ChipTestBoard) {
        throw std::runtime_error(LOCATION + "Not a Ctb file");
    }

    find_subfiles();

    // open the first subfile
    m_file.open(m_master.data_fname(0, 0), std::ios::binary);
}

void CtbRawFile::read_into(std::byte *image_buf, DetectorHeader *header) {
    if (m_current_frame >= m_master.frames_in_file()) {
        throw std::runtime_error(LOCATION + " End of file reached");
    }

    if (m_current_frame != 0 &&
        m_current_frame % m_master.max_frames_per_file() == 0) {
        open_data_file(m_current_subfile + 1);
    }

    if (header) {
        m_file.read(reinterpret_cast<char *>(header), sizeof(DetectorHeader));
    } else {
        m_file.seekg(sizeof(DetectorHeader), std::ios::cur);
    }

    m_file.read(reinterpret_cast<char *>(image_buf),
                m_master.image_size_in_bytes());
    m_current_frame++;
}

void CtbRawFile::seek(size_t frame_number) {
    if (auto index = sub_file_index(frame_number); index != m_current_subfile) {
        open_data_file(index);
    }
    size_t frame_number_in_file = frame_number % m_master.max_frames_per_file();
    m_file.seekg((sizeof(DetectorHeader) + m_master.image_size_in_bytes()) *
                 frame_number_in_file);
    m_current_frame = frame_number;
}

size_t CtbRawFile::tell() const { return m_current_frame; }

size_t CtbRawFile::image_size_in_bytes() const {
    return m_master.image_size_in_bytes();
}

size_t CtbRawFile::frames_in_file() const { return m_master.frames_in_file(); }

RawMasterFile CtbRawFile::master() const { return m_master; }

void CtbRawFile::find_subfiles() {
    // we can semi safely assume that there is only one module for CTB
    while (std::filesystem::exists(m_master.data_fname(0, m_num_subfiles)))
        m_num_subfiles++;

    fmt::print("Found {} subfiles\n", m_num_subfiles);
}

void CtbRawFile::open_data_file(size_t subfile_index) {
    if (subfile_index >= m_num_subfiles) {
        throw std::runtime_error(LOCATION + "Subfile index out of range");
    }
    m_current_subfile = subfile_index;
    m_file = std::ifstream(m_master.data_fname(0, subfile_index),
                           std::ios::binary); // only one module for CTB
    if (!m_file.is_open()) {
        throw std::runtime_error(LOCATION + "Could not open data file");
    }
}

} // namespace aare