#include "aare/file_io/SubFile.hpp"
#include "aare/utils/logger.hpp"
#include <cstring> // memcpy
#include <fmt/core.h>
#include <iostream>
// #include <filesystem>

namespace aare {

SubFile::SubFile(const std::filesystem::path &fname,[[maybe_unused]] DetectorType detector, size_t rows, size_t cols, size_t bitdepth,
                 const std::string &mode)
    : m_bitdepth(bitdepth), m_fname(fname), m_rows(rows), m_cols(cols), m_mode(mode) {

    if (std::filesystem::exists(fname)) {
        n_frames = std::filesystem::file_size(fname) / (sizeof(sls_detector_header) + rows * cols * bitdepth / 8);
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
    fseek(fp, sizeof(sls_detector_header) + (sizeof(sls_detector_header) + bytes_per_part()) * frame_index, // NOLINT
          SEEK_SET);
    return fread(buffer, this->bytes_per_part(), 1, this->fp);
}
size_t SubFile::write_part(std::byte *buffer, sls_detector_header header, size_t frame_index) {
    if (frame_index > n_frames) {
        throw std::runtime_error("Frame number out of range");
    }
    fseek(fp, static_cast<int64_t>((sizeof(sls_detector_header) + bytes_per_part()) * frame_index), SEEK_SET);
    auto wc = fwrite(reinterpret_cast<char *>(&header), sizeof(header), 1, fp);
    wc += fwrite(buffer, bytes_per_part(), 1, fp);

    return wc;
}

size_t SubFile::frame_number(size_t frame_index) {
    sls_detector_header h{};
    fseek(fp, (sizeof(sls_detector_header) + bytes_per_part()) * frame_index, SEEK_SET); // NOLINT
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