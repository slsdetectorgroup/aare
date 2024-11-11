#include "aare/RawSubFile.hpp"
#include "aare/PixelMap.hpp"
#include <cstring> // memcpy
#include <fmt/core.h>
#include <iostream>

namespace aare {

RawSubFile::RawSubFile(const std::filesystem::path &fname,
                       DetectorType detector, size_t rows, size_t cols,
                       size_t bitdepth)
    : m_bitdepth(bitdepth), m_fname(fname), m_rows(rows), m_cols(cols),
      m_detector_type(detector),
      m_bytes_per_frame((m_bitdepth / 8) * m_rows * m_cols) {
    if (m_detector_type == DetectorType::Moench03_old) {
        pixel_map = GenerateMoench03PixelMap();
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
        throw std::runtime_error("Frame number out of range");
    }
    m_file.seekg((sizeof(DetectorHeader) + bytes_per_frame()) * frame_index);
}

size_t RawSubFile::tell() {
    return m_file.tellg() / (sizeof(DetectorHeader) + bytes_per_frame());
}


void RawSubFile::read_into(std::byte *image_buf, DetectorHeader *header) {
    if(header){
         m_file.read(reinterpret_cast<char *>(header), sizeof(DetectorHeader));
    } else {
        m_file.seekg(sizeof(DetectorHeader), std::ios::cur);
    }

    //TODO! expand support for different bitdepths
    if(pixel_map){
        // read into a temporary buffer and then copy the data to the buffer
        // in the correct order
        // currently this only supports 16 bit data!
        auto part_buffer = new std::byte[bytes_per_frame()];
        m_file.read(reinterpret_cast<char *>(part_buffer), bytes_per_frame());
        auto *data = reinterpret_cast<uint16_t *>(image_buf);
        auto *part_data = reinterpret_cast<uint16_t *>(part_buffer);
        for (size_t i = 0; i < pixels_per_frame(); i++) {
            data[i] = part_data[(*pixel_map)(i)];
        }
        delete[] part_buffer;
    } else {
        // read directly into the buffer
        m_file.read(reinterpret_cast<char *>(image_buf), bytes_per_frame());
    }
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