#include "aare/file_io/SubFile.hpp"
#include "aare/utils/logger.hpp"
#include <cstring> // memcpy
#include <fmt/core.h>
#include <iostream>
// #include <filesystem>

namespace aare {

SubFile::SubFile(const std::filesystem::path &fname, DetectorType detector, ssize_t rows, ssize_t cols,
                 uint16_t bitdepth)
    : m_bitdepth(bitdepth), m_fname(fname), m_rows(rows), m_cols(cols),
      n_frames(std::filesystem::file_size(fname) / (sizeof(sls_detector_header) + rows * cols * bitdepth / 8)) {

    if (read_impl_map.find({detector, bitdepth}) == read_impl_map.end()) {
        auto error_msg = LOCATION + "No read_impl function found for detector: " + toString(detector) +
                         " and bitdepth: " + std::to_string(bitdepth);
        throw std::invalid_argument(error_msg);
    }
    this->read_impl = read_impl_map.at({detector, bitdepth});
}

size_t SubFile::get_part(std::byte *buffer, size_t frame_number) {
    if (frame_number >= n_frames or frame_number < 0) {
        throw std::runtime_error("Frame number out of range");
    }
    // TODO: find a way to avoid opening and closing the file for each frame
    aare::logger::debug(LOCATION, "frame:", frame_number, "file:", m_fname.c_str());
    fp = fopen(m_fname.c_str(), "rb");
    if (!fp) {
        throw std::runtime_error(fmt::format("Could not open: {} for reading", m_fname.c_str()));
    }
    fseek(fp, sizeof(sls_detector_header) + (sizeof(sls_detector_header) + bytes_per_part()) * frame_number, // NOLINT
          SEEK_SET);
    auto ret = (this->*read_impl)(buffer);
    if (!fclose(fp))
        throw std::runtime_error(LOCATION + "Could not close file");
    return ret;
}

size_t SubFile::read_impl_normal(std::byte *buffer) { return fread(buffer, this->bytes_per_part(), 1, this->fp); }

template <typename DataType> size_t SubFile::read_impl_reorder(std::byte *buffer) {

    std::vector<DataType> tmp(this->pixels_per_part());
    size_t const rc = fread(reinterpret_cast<char *>(tmp.data()), this->bytes_per_part(), 1, this->fp);

    std::array<int, 32> const adc_nr = {300, 325, 350, 375, 300, 325, 350, 375, 200, 225, 250, 275, 200, 225, 250, 275,
                                        100, 125, 150, 175, 100, 125, 150, 175, 0,   25,  50,  75,  0,   25,  50,  75};
    int const sc_width = 25;
    int const nadc = 32;
    int const pixels_per_sc = 5000;

    auto dst = reinterpret_cast<DataType *>(buffer);
    int pixel = 0;
    for (int i = 0; i != pixels_per_sc; ++i) {
        for (int i_adc = 0; i_adc != nadc; ++i_adc) {
            int const col = adc_nr[i_adc] + (i % sc_width);
            int row = 0;
            if ((i_adc / 4) % 2 == 0)
                row = 199 - (i / sc_width);
            else
                row = 200 + (i / sc_width);

            dst[col + row * 400] = tmp[pixel];
            pixel++;
        }
    }
    return rc;
};
template <typename DataType> size_t SubFile::read_impl_flip(std::byte *buffer) {

    // read to temporary buffer
    // TODO! benchmark direct reads
    std::vector<std::byte> tmp(this->bytes_per_part());
    size_t const rc = fread(reinterpret_cast<char *>(tmp.data()), this->bytes_per_part(), 1, this->fp);

    // copy to place
    const size_t start = this->m_cols * (this->m_rows - 1) * sizeof(DataType);
    const size_t row_size = this->m_cols * sizeof(DataType);
    auto *dst = buffer + start;
    auto *src = tmp.data();

    for (size_t i = 0; i != this->m_rows; ++i) {
        std::memcpy(dst, src, row_size);
        dst -= row_size;
        src += row_size;
    }

    return rc;
};

size_t SubFile::frame_number(int frame_index) {
    sls_detector_header h{};
    fp = fopen(this->m_fname.c_str(), "r");
    if (!fp)
        throw std::runtime_error(LOCATION + fmt::format("Could not open: {} for reading", m_fname.c_str()));
    fseek(fp, (sizeof(sls_detector_header) + bytes_per_part()) * frame_index, SEEK_SET); // NOLINT
    size_t const rc = fread(reinterpret_cast<char *>(&h), sizeof(h), 1, fp);
    if (rc != sizeof(h))
        throw std::runtime_error(LOCATION + "Could not read header from file");
    if (!fclose(fp)) {
        throw std::runtime_error(LOCATION + "Could not close file");
    }

    return h.frameNumber;
}

} // namespace aare