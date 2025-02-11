#include "aare/RawSubFile.hpp"
#include "aare/PixelMap.hpp"
#include <cstring> // memcpy
#include <fmt/core.h>
#include <iostream>

namespace aare {

RawSubFile::RawSubFile(const std::filesystem::path &fname,
                       DetectorType detector, size_t rows, size_t cols,
                       size_t bitdepth, uint32_t pos_row, uint32_t pos_col)
    : m_detector_type(detector), m_bitdepth(bitdepth), m_fname(fname),
      m_rows(rows), m_cols(cols),
      m_bytes_per_frame((m_bitdepth / 8) * m_rows * m_cols), m_pos_row(pos_row),
      m_pos_col(pos_col) {
    if (m_detector_type == DetectorType::Moench03_old) {
        m_pixel_map = GenerateMoench03PixelMap();
    } else if (m_detector_type == DetectorType::Eiger && m_pos_row % 2 == 0) {
        m_pixel_map = GenerateEigerFlipRowsPixelMap();
    }

    if (std::filesystem::exists(fname)) {
        n_frames = std::filesystem::file_size(fname) /
                   (sizeof(DetectorHeader) + rows * cols * bitdepth / 8);
    } else {
        throw std::runtime_error(
            LOCATION + fmt::format("File {} does not exist", m_fname.string()));
    }

    // fp = fopen(m_fname.string().c_str(), "rb");
    m_file.open(m_fname, std::ios::binary);
    if (!m_file.is_open()) {
        throw std::runtime_error(
            LOCATION + fmt::format("Could not open file {}", m_fname.string()));
    }

#ifdef AARE_VERBOSE
    fmt::print("Opened file: {} with {} frames\n", m_fname.string(), n_frames);
    fmt::print("m_rows: {}, m_cols: {}, m_bitdepth: {}\n", m_rows, m_cols,
               m_bitdepth);
    fmt::print("file size: {}\n", std::filesystem::file_size(fname));
#endif
}

void RawSubFile::seek(size_t frame_index) {
    if (frame_index >= n_frames) {
        throw std::runtime_error(LOCATION + fmt::format("Frame index {} out of range in a file with {} frames", frame_index, n_frames));
    }
    m_file.seekg((sizeof(DetectorHeader) + bytes_per_frame()) * frame_index);
}

size_t RawSubFile::tell() {
    return m_file.tellg() / (sizeof(DetectorHeader) + bytes_per_frame());
}

void RawSubFile::read_into(std::byte *image_buf, DetectorHeader *header) {
    if (header) {
        m_file.read(reinterpret_cast<char *>(header), sizeof(DetectorHeader));
    } else {
        m_file.seekg(sizeof(DetectorHeader), std::ios::cur);
    }

    // TODO! expand support for different bitdepths
    if (m_pixel_map) {
        // read into a temporary buffer and then copy the data to the buffer
        // in the correct order
        // TODO! add 4 bit support
        if(m_bitdepth == 8){
            read_with_map<uint8_t>(image_buf);
        }else if (m_bitdepth == 16) {
            read_with_map<uint16_t>(image_buf);
        } else if (m_bitdepth == 32) {
            read_with_map<uint32_t>(image_buf);
        }else{
            throw std::runtime_error("Unsupported bitdepth for read with pixel map");
        }

    } else {
        // read directly into the buffer
        m_file.read(reinterpret_cast<char *>(image_buf), bytes_per_frame());
    }
}

template <typename T>
void RawSubFile::read_with_map(std::byte *image_buf) {
    auto part_buffer = new std::byte[bytes_per_frame()];
    m_file.read(reinterpret_cast<char *>(part_buffer), bytes_per_frame());
    auto *data = reinterpret_cast<T *>(image_buf);
    auto *part_data = reinterpret_cast<T *>(part_buffer);
    for (size_t i = 0; i < pixels_per_frame(); i++) {
        data[i] = part_data[(*m_pixel_map)(i)];
    }
    delete[] part_buffer;
}
size_t RawSubFile::rows() const { return m_rows; }
size_t RawSubFile::cols() const { return m_cols; }

void RawSubFile::get_part(std::byte *buffer, size_t frame_index) {
    seek(frame_index);
    read_into(buffer, nullptr);
}

size_t RawSubFile::frame_number(size_t frame_index) {
    seek(frame_index);
    DetectorHeader h{};
    m_file.read(reinterpret_cast<char *>(&h), sizeof(DetectorHeader));
    return h.frameNumber;
}

} // namespace aare