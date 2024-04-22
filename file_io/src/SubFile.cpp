#include "aare/file_io/SubFile.hpp"
#include "aare/utils/logger.hpp"
#include <cstring> // memcpy
#include <fmt/core.h>
#include <iostream>
// #include <filesystem>

namespace aare {

SubFile::SubFile(const std::filesystem::path &fname, DetectorType detector, size_t rows, size_t cols, size_t bitdepth,
                 int subfile_id)
    : m_bitdepth(bitdepth), m_fname(fname), m_rows(rows), m_cols(cols),
      n_frames(std::filesystem::file_size(fname) / (sizeof(sls_detector_header) + rows * cols * bitdepth / 8)),
      m_subfile_id(subfile_id) {

    if (read_impl_map.find({detector, bitdepth}) == read_impl_map.end()) {
        auto error_msg = LOCATION + "No read_impl function found for detector: " + toString(detector) +
                         " and bitdepth: " + std::to_string(bitdepth);
        throw std::invalid_argument(error_msg);
    }
    this->read_impl = read_impl_map.at({detector, bitdepth});

    if (!(fp = fopen(fname.c_str(), "rb"))) {
        throw std::runtime_error(fmt::format(LOCATION + "Could not open: {} for reading", fname.c_str()));
    }
}

size_t SubFile::correct_frame_number(size_t frame_number) {
    sls_detector_header h{};
    for (size_t offset = cached_offset; offset < n_frames; offset++) {
        size_t ret = (frame_number + offset) % n_frames;
        fseek(fp, (sizeof(sls_detector_header) + bytes_per_part()) * ret, SEEK_SET); // NOLINT
        size_t const rc = fread(reinterpret_cast<char *>(&h), sizeof(h), 1, fp);
        if (rc != 1)
            throw std::runtime_error(LOCATION + "Could not read header from file");
        if (h.frameNumber == frame_number) {
            cached_offset = offset;
            return ret;
        }
    }
    aare::logger::error("m_subfile_id:", m_subfile_id);
    aare::logger::error(LOCATION, ": frame:", h.frameNumber, "frame_number:", frame_number, "toto",
                        (h.frameNumber - (m_subfile_id * n_frames)));
    throw std::runtime_error(fmt::format(LOCATION + "Could not find frame {} in file", frame_number));
}

size_t SubFile::get_part(std::byte *buffer, size_t frame_number) {
    aare::logger::debug(LOCATION, "frame:", frame_number, "file:", m_fname.c_str());
    size_t tmp = correct_frame_number(frame_number);
    fseek(fp, sizeof(sls_detector_header) + (sizeof(sls_detector_header) + bytes_per_part()) * tmp, SEEK_SET); // NOLINT
    auto ret = (this->*read_impl)(buffer);
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

size_t SubFile::frame_number_in_file(size_t frame_index) {
    sls_detector_header h{};
    fseek(fp, (sizeof(sls_detector_header) + bytes_per_part()) * frame_index, SEEK_SET); // NOLINT
    size_t const rc = fread(reinterpret_cast<char *>(&h), sizeof(h), 1, fp);
    if (rc != 1)
        throw std::runtime_error(LOCATION + "Could not read header from file");
    return h.frameNumber;
}

SubFile::~SubFile() noexcept {
    if (fp) {
        if (fclose(fp))
            aare::logger::error(LOCATION, "Could not close file");
    }
}

} // namespace aare