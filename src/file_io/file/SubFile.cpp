#include "SubFile.hpp"
#include <iostream>
// #include <filesystem>

/**
 * SubFile methods
 *
 *
 */
template <class Header, class DataType>
SubFile<Header, DataType>::SubFile(std::filesystem::path fname, ssize_t rows, ssize_t cols)  {
    this->rows = rows;
    this->cols = cols;
    this->fname = fname;
    fp = fopen(fname.c_str(), "rb");
    if (fp == nullptr) {
        throw std::runtime_error("Could not open file " + fname.string());
    }
    std::cout<<"File opened"<<std::endl;
    n_frames = std::filesystem::file_size(fname) / (sizeof(Header) + rows * cols * sizeof(DataType));
    std::cout<<"Number of frames: "<<n_frames<<std::endl;
}


template <class Header, class DataType>
size_t SubFile<Header, DataType>::get_frame(std::byte *buffer, int frame_number) {
    if (frame_number >= n_frames or frame_number < 0) {
        throw std::runtime_error("Frame number out of range");
    }
    fseek(fp, sizeof(Header)+(sizeof(Header) + bytes_per_frame()) *frame_number, SEEK_SET);
    return read_impl(buffer);
}

/**
 * NormalSubFile methods
*/
template <class Header, class DataType>
NormalSubFile<Header, DataType>::NormalSubFile(std::filesystem::path fname, ssize_t rows, ssize_t cols)
    : SubFile<Header, DataType>(fname, rows, cols){};

template <class Header, class DataType> size_t NormalSubFile<Header, DataType>::read_impl(std::byte *buffer) {
    return fread(buffer, this->bytes_per_frame(), 1, this->fp);
};



/**
 * ReorderM03SubFile methods
*/
template <class Header, class DataType>
ReorderM03SubFile<Header, DataType>::ReorderM03SubFile(std::filesystem::path fname, ssize_t rows, ssize_t cols)
    : SubFile<Header, DataType>(fname, rows, cols){};

template <class Header, class DataType> 
size_t ReorderM03SubFile<Header, DataType>::read_impl(std::byte *buffer) {
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

template class NormalSubFile<sls_detector_header, uint16_t>;
template class NormalSubFile<sls_detector_header, uint32_t>;
template class ReorderM03SubFile<sls_detector_header, uint16_t>;

// template size_t ReorderM03SubFile<sls_detector_header, uint16_t>::read_impl(std::byte *buffer); 
// template size_t ReorderM03SubFile<sls_detector_header, uint32_t>::read_impl(std::byte *buffer); 

// template size_t NormalSubFile<sls_detector_header, uint32_t>::read_impl(std::byte *buffer);
// template size_t NormalSubFile<sls_detector_header, uint16_t>::read_impl(std::byte *buffer);