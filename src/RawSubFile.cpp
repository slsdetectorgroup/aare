#include "aare/RawSubFile.hpp"
#include "aare/PixelMap.hpp"
#include "aare/algorithm.hpp"
#include "aare/logger.hpp"
#include "aare/utils/ifstream_helpers.hpp"

#include <cstring> // memcpy
#include <fmt/core.h>
#include <iostream>
#include <regex>

namespace aare {

RawSubFile::RawSubFile(const std::filesystem::path &fname,
                       DetectorType detector, size_t rows, size_t cols,
                       size_t bitdepth, uint32_t pos_row, uint32_t pos_col)
    : m_detector_type(detector), m_bitdepth(bitdepth), m_rows(rows),
      m_cols(cols), m_bytes_per_frame((m_bitdepth / 8) * m_rows * m_cols),
      m_pos_row(pos_row), m_pos_col(pos_col) {

    LOG(logDEBUG) << "RawSubFile::RawSubFile()";
    if (m_detector_type == DetectorType::Moench03_old) {
        m_pixel_map = GenerateMoench03PixelMap();
    } else if (m_detector_type == DetectorType::Eiger && m_pos_row % 2 == 0) {
        m_pixel_map = GenerateEigerFlipRowsPixelMap();
    }

    parse_fname(fname);
    scan_files();
    open_file(m_current_file_index); // open the first file
}

void RawSubFile::seek(size_t frame_index) {
    LOG(logDEBUG) << "RawSubFile::seek(" << frame_index << ")";
    if (frame_index >= m_total_frames) {
        throw std::runtime_error(LOCATION + " Frame index out of range: " +
                                 std::to_string(frame_index));
    }
    m_current_frame_index = frame_index;
    auto file_index = first_larger(m_last_frame_in_file, frame_index);

    if (file_index != m_current_file_index)
        open_file(file_index);

    auto frame_offset = (file_index)
                            ? frame_index - m_last_frame_in_file[file_index - 1]
                            : frame_index;
    auto byte_offset =
        frame_offset * (m_bytes_per_frame + sizeof(DetectorHeader));
    m_file.seekg(byte_offset);
}

size_t RawSubFile::tell() {
    LOG(logDEBUG) << "RawSubFile::tell():" << m_current_frame_index;
    return m_current_frame_index;
}

void RawSubFile::read_into(std::byte *image_buf, DetectorHeader *header) {
    LOG(logDEBUG) << "RawSubFile::read_into()";

    if (header) {
        m_file.read(reinterpret_cast<char *>(header), sizeof(DetectorHeader));
    } else {
        m_file.seekg(sizeof(DetectorHeader), std::ios::cur);
    }

    if (m_file.fail()) {
        throw std::runtime_error(LOCATION + ifstream_error_msg(m_file));
    }

    // TODO! expand support for different bitdepths
    if (m_pixel_map) {
        // read into a temporary buffer and then copy the data to the buffer
        // in the correct order
        // TODO! add 4 bit support
        if (m_bitdepth == 8) {
            read_with_map<uint8_t>(image_buf);
        } else if (m_bitdepth == 16) {
            read_with_map<uint16_t>(image_buf);
        } else if (m_bitdepth == 32) {
            read_with_map<uint32_t>(image_buf);
        } else {
            throw std::runtime_error(
                "Unsupported bitdepth for read with pixel map");
        }

    } else {
        // read directly into the buffer
        m_file.read(reinterpret_cast<char *>(image_buf), bytes_per_frame());
    }

    if (m_file.fail()) {
        throw std::runtime_error(LOCATION + ifstream_error_msg(m_file));
    }

    ++m_current_frame_index;
    if (m_current_frame_index >= m_last_frame_in_file[m_current_file_index] &&
        (m_current_frame_index < m_total_frames)) {
        ++m_current_file_index;
        open_file(m_current_file_index);
    }
}

void RawSubFile::read_into(std::byte *image_buf, size_t n_frames,
                           DetectorHeader *header) {
    for (size_t i = 0; i < n_frames; i++) {
        read_into(image_buf, header);
        image_buf += bytes_per_frame();
        if (header) {
            ++header;
        }
    }
}

template <typename T> void RawSubFile::read_with_map(std::byte *image_buf) {
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

void RawSubFile::parse_fname(const std::filesystem::path &fname) {
    LOG(logDEBUG) << "RawSubFile::parse_fname()";
    // data has the format: /path/too/data/jungfrau_single_d0_f1_0.raw
    // d0 is the module index, will not change for this file
    // f1 is the file index - thi is the one we need
    // 0 is the measurement index, will not change
    m_path = fname.parent_path();
    m_base_name = fname.filename();

    // Regex to extract numbers after 'd' and 'f'
    std::regex pattern(R"(^(.*_d)(\d+)(_f)(\d+)(_\d+\.raw)$)");
    std::smatch match;

    if (std::regex_match(m_base_name, match, pattern)) {
        m_offset = std::stoi(match[4].str()); // find the first file index in
                                              // case of a truncated series
        m_base_name = match[1].str() + match[2].str() + match[3].str() + "{}" +
                      match[5].str();
        LOG(logDEBUG) << "Base name: " << m_base_name;
        LOG(logDEBUG) << "Offset: " << m_offset;
        LOG(logDEBUG) << "Path: " << m_path.string();
    } else {
        throw std::runtime_error(
            LOCATION +
            fmt::format("Could not parse file name {}", fname.string()));
    }
}

std::filesystem::path RawSubFile::fpath(size_t file_index) const {
    auto fname = fmt::format(m_base_name, file_index);
    return m_path / fname;
}

void RawSubFile::open_file(size_t file_index) {
    m_file.close();
    auto fname = fpath(file_index + m_offset);
    LOG(logDEBUG) << "RawSubFile::open_file():  " << fname.string();
    m_file.open(fname, std::ios::binary);
    if (!m_file.is_open()) {
        throw std::runtime_error(
            LOCATION +
            fmt::format("Could not open file {}", fpath(file_index).string()));
    }
    m_current_file_index = file_index;
}

void RawSubFile::scan_files() {
    LOG(logDEBUG) << "RawSubFile::scan_files()";
    // find how many files we have and the number of frames in each file
    m_last_frame_in_file.clear();
    size_t file_index = m_offset;

    while (std::filesystem::exists(fpath(file_index))) {
        auto n_frames = std::filesystem::file_size(fpath(file_index)) /
                        (m_bytes_per_frame + sizeof(DetectorHeader));
        m_last_frame_in_file.push_back(n_frames);
        LOG(logDEBUG) << "Found: " << n_frames
                      << " frames in file: " << fpath(file_index).string();
        ++file_index;
    }

    // find where we need to open the next file and total number of frames
    m_last_frame_in_file = cumsum(m_last_frame_in_file);
    if (m_last_frame_in_file.empty()) {
        m_total_frames = 0;
    } else {
        m_total_frames = m_last_frame_in_file.back();
    }
}

} // namespace aare