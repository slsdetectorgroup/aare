#include "aare/ImageData.hpp"

template <typename DataType> ImageData<DataType>::ImageData(ssize_t rows, ssize_t cols) {
    this->m_rows = rows;
    this->m_cols = cols;

    this->m_data = new DataType[rows * cols];
}

template <typename DataType> ImageData<DataType>::ImageData(std::byte *bytes, ssize_t rows, ssize_t cols) {
    this->m_rows = rows;
    this->m_cols = cols;

    this->m_data = new DataType[rows * cols];
    std::memcpy(this->m_data, bytes, this->m_rows * this->m_cols * sizeof(DataType));
}

template <typename DataType> void ImageData<DataType>::_copy(IFrame<DataType>& frame) {
        // If the data is not allocated, allocate it
    if (this->m_data == nullptr) {
        this->m_data = new DataType[frame.rows() * frame.cols()];
    }
    // If the data is allocated but the size is different, reallocate it
    else if ( this->rows() * this->cols() != frame.rows() * frame.cols()) {
        delete[] this->m_data;
        this->m_data = new DataType[frame.rows() * frame.cols()];
    }
    // Copy the data
    std::memcpy(this->m_data, frame._get_data(), frame.rows() * frame.cols() * sizeof(DataType));
    this->m_rows = frame.rows();
    this->m_cols = frame.cols();
}

template <typename DataType> ImageData<DataType>::ImageData(IFrame<DataType>& frame) {
    this->_copy(frame);
}

template <typename DataType> ImageData<DataType>& ImageData<DataType>::operator=(IFrame<DataType>& frame) {
    this->_copy(frame);
    return *this;
}

template class ImageData<uint8_t>;
template class ImageData<uint16_t>;
template class ImageData<uint32_t>;