#include "aare/SubFile.hpp"
#include "aare/PixelMap.hpp"
#include <cstring> // memcpy
#include <fmt/core.h>
#include <iostream>


namespace aare {

SubFile::SubFile(const std::filesystem::path &fname, DetectorType detector, size_t rows, size_t cols, size_t bitdepth,
                 const std::string &mode)
    : m_bitdepth(bitdepth), m_fname(fname), m_rows(rows), m_cols(cols), m_mode(mode), m_detector_type(detector) {


    if (m_detector_type == DetectorType::Moench03_old) {
        pixel_map = GenerateMoench03PixelMap();
    }

    if (std::filesystem::exists(fname)) {
        n_frames = std::filesystem::file_size(fname) / (sizeof(DetectorHeader) + rows * cols * bitdepth / 8);
    } else {
        n_frames = 0;
    }

    if (mode == "r") {
        fp = fopen(m_fname.string().c_str(), "rb");
    } else {
        // if file exists, open in read/write mode (without truncating the file)
        // if file does not exist, open in write mode
        if (std::filesystem::exists(fname)) {
            fp = fopen(m_fname.string().c_str(), "r+b");
        } else {
            fp = fopen(m_fname.string().c_str(), "wb");
        }
    }
    if (fp == nullptr) {
        throw std::runtime_error(LOCATION + "Could not open file for writing");
    }
}

size_t SubFile::get_part(std::byte *buffer, size_t frame_index) {
    if (frame_index >= n_frames) {
        throw std::runtime_error("Frame number out of range");
    }
    fseek(fp, sizeof(DetectorHeader) + (sizeof(DetectorHeader) + bytes_per_part()) * frame_index, // NOLINT
          SEEK_SET);

    if (pixel_map){
        // read into a temporary buffer and then copy the data to the buffer
        // in the correct order
        auto part_buffer = new std::byte[bytes_per_part()];
        auto wc = fread(part_buffer, bytes_per_part(), 1, fp);
        auto *data = reinterpret_cast<uint16_t *>(buffer);
        auto *part_data = reinterpret_cast<uint16_t *>(part_buffer);
        for (size_t i = 0; i < pixels_per_part(); i++) {
            data[i] = part_data[(*pixel_map)(i)];
        }
        delete[] part_buffer;
        return wc;
    }else{
        // read directly into the buffer
        return fread(buffer, this->bytes_per_part(), 1, this->fp);
    }
    
}
size_t SubFile::write_part(std::byte *buffer, DetectorHeader header, size_t frame_index) {
    if (frame_index > n_frames) {
        throw std::runtime_error("Frame number out of range");
    }
    fseek(fp, static_cast<int64_t>((sizeof(DetectorHeader) + bytes_per_part()) * frame_index), SEEK_SET);
    auto wc = fwrite(reinterpret_cast<char *>(&header), sizeof(header), 1, fp);
    wc += fwrite(buffer, bytes_per_part(), 1, fp);

    return wc;
}

size_t SubFile::frame_number(size_t frame_index) {
    DetectorHeader h{};
    fseek(fp, (sizeof(DetectorHeader) + bytes_per_part()) * frame_index, SEEK_SET); // NOLINT
    size_t const rc = fread(reinterpret_cast<char *>(&h), sizeof(h), 1, fp);
    if (rc != 1)
        throw std::runtime_error(LOCATION + "Could not read header from file");

    return h.frameNumber;
}

SubFile::~SubFile() {
    if (fp) {
        fclose(fp);
    }
}

} // namespace aare