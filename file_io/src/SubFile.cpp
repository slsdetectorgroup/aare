#include "aare/SubFile.hpp"
#include <iostream>
// #include <filesystem>

/**
 * SubFile methods
 *
 *
 */

SubFile::SubFile(std::filesystem::path fname, DetectorType detector, ssize_t rows, ssize_t cols, uint16_t bitdepth) {
    this->rows = rows;
    this->cols = cols;
    this->fname = fname;
    this->bitdepth = bitdepth;
    fp = fopen(fname.c_str(), "rb");
    if (fp == nullptr) {
        throw std::runtime_error("Could not open file " + fname.string());
    }
    std::cout << "File opened" << std::endl;
    n_frames = std::filesystem::file_size(fname) / (sizeof(sls_detector_header) + rows * cols * bitdepth / 8);
    std::cout << "Number of frames: " << n_frames << std::endl;
    
    if (detector == DetectorType::Moench) {
        read_impl = &SubFile::read_impl_reorder<uint16_t>;
    } else if (detector == DetectorType::Jungfrau) {
        read_impl = &SubFile::read_impl_normal;
    }
    else {
        throw std::runtime_error("Detector type not implemented");
    }
}

size_t SubFile::get_frame(std::byte *buffer, int frame_number) {
    if (frame_number >= n_frames or frame_number < 0) {
        throw std::runtime_error("Frame number out of range");
    }
    fseek(fp, sizeof(sls_detector_header) + (sizeof(sls_detector_header) + bytes_per_frame()) * frame_number, SEEK_SET);
    return (this->*read_impl)(buffer);
}

size_t SubFile::read_impl_normal(std::byte *buffer) { return fread(buffer, this->bytes_per_frame(), 1, this->fp); }

template <typename DataType> size_t SubFile::read_impl_reorder(std::byte *buffer) {

    std::vector<DataType> tmp(this->pixels_per_frame());
    size_t rc = fread(reinterpret_cast<char *>(&tmp[0]), this->bytes_per_frame(), 1, this->fp);

    int adc_nr[32] = {300, 325, 350, 375, 300, 325, 350, 375, 200, 225, 250, 275, 200, 225, 250, 275,
                      100, 125, 150, 175, 100, 125, 150, 175, 0,   25,  50,  75,  0,   25,  50,  75};
    int sc_width = 25;
    int nadc = 32;
    int pixels_per_sc = 5000;

    auto dst = reinterpret_cast<DataType *>(buffer);
    int pixel = 0;
    for (int i = 0; i != pixels_per_sc; ++i) {
        for (int i_adc = 0; i_adc != nadc; ++i_adc) {
            int col = adc_nr[i_adc] + (i % sc_width);
            int row;
            if ((i_adc / 4) % 2 == 0)
                row = 199 - int(i / sc_width);
            else
                row = 200 + int(i / sc_width);

            dst[col + row * 400] = tmp[pixel];
            pixel++;
        }
    }
    return rc;
};
template <typename DataType> size_t SubFile::read_impl_flip(std::byte *buffer) {

    // read to temporary buffer
    // TODO! benchmark direct reads
    std::vector<std::byte> tmp(this->bytes_per_frame());
    size_t rc = fread(reinterpret_cast<char *>(&tmp[0]), this->bytes_per_frame(), 1, this->fp);

    // copy to place
    const size_t start = this->cols * (this->rows - 1) * sizeof(DataType);
    const size_t row_size = this->cols * sizeof(DataType);
    auto dst = buffer + start;
    auto src = &tmp[0];

    for (int i = 0; i != this->rows; ++i) {
        memcpy(dst, src, row_size);
        dst -= row_size;
        src += row_size;
    }

    return rc;
};
