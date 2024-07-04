#include "aare/core/Frame.hpp"
#include <cstddef>
#include <cstring>
#include <iostream>
#include <sys/types.h>

namespace aare {

/**
 * @brief Construct a new Frame
 * @param bytes pointer to the data to be copied into the frame
 * @param rows number of rows
 * @param cols number of columns
 * @param bitdepth bitdepth of the pixels
 */
Frame::Frame(const std::byte *bytes, uint32_t rows, uint32_t cols, Dtype dtype)
    : m_rows(rows), m_cols(cols), m_dtype(dtype), m_data(new std::byte[rows * cols * m_dtype.bytes()]) {

    std::memcpy(m_data, bytes, rows * cols * m_dtype.bytes());
}

/**
 * @brief Construct a new Frame
 * @param rows number of rows
 * @param cols number of columns
 * @param bitdepth bitdepth of the pixels
 * @note the data is initialized to zero
 */
Frame::Frame(uint32_t rows, uint32_t cols, Dtype dtype)
    : m_rows(rows), m_cols(cols), m_dtype(dtype), m_data(new std::byte[rows * cols * dtype.bytes()]) {

    std::memset(m_data, 0, rows * cols * dtype.bytes());
}

uint32_t Frame::rows() const { return m_rows; }
uint32_t Frame::cols() const { return m_cols; }
size_t Frame::bitdepth() const { return m_dtype.bitdepth(); }
Dtype Frame::dtype() const { return m_dtype; }
uint64_t Frame::size() const { return m_rows * m_cols; }
size_t Frame::bytes() const { return m_rows * m_cols * m_dtype.bytes(); }
std::byte *Frame::data() const { return m_data; }

/**
 * @brief Get the pointer to the pixel at the given row and column
 * @param row row index
 * @param col column index
 * @return pointer to the pixel
 * @note the user should cast the pointer to the appropriate type
 */
std::byte *Frame::get(uint32_t row, uint32_t col) {
    if ((row >= m_rows) || (col >= m_cols)) {
        std::cerr << "Invalid row or column index" << '\n';
        return nullptr;
    }
    return m_data + (row * m_cols + col) * (m_dtype.bytes());
}

// Frame &Frame::operator=(const Frame &other) {
//     if (this == &other) {
//         return *this;
//     }
//     m_rows = other.rows();
//     m_cols = other.cols();
//     m_dtype = other.dtype();
//     m_data = new std::byte[m_rows * m_cols * m_dtype.bytes()];
//     if (m_data == nullptr) {
//         throw std::bad_alloc();
//     }
//     std::memcpy(m_data, other.m_data, m_rows * m_cols * m_dtype.bytes());
//     return *this;
// }
Frame &Frame::operator=(Frame &&other) noexcept {
    if (this == &other) {
        return *this;
    }
    m_rows = other.rows();
    m_cols = other.cols();
    m_dtype = other.dtype();
    if (m_data != nullptr) {
        delete[] m_data;
    }
    m_data = other.m_data;
    other.m_data = nullptr;
    other.m_rows = other.m_cols = 0;
    other.m_dtype = Dtype(Dtype::TypeIndex::ERROR);
    return *this;
}
Frame::Frame(Frame &&other) noexcept
    : m_rows(other.rows()), m_cols(other.cols()), m_dtype(other.dtype()), m_data(other.m_data) {

    other.m_data = nullptr;
    other.m_rows = other.m_cols = 0;
    other.m_dtype = Dtype(Dtype::TypeIndex::ERROR);
}
// Frame::Frame(const Frame &other)
//     : m_rows(other.rows()), m_cols(other.cols()), m_dtype(other.dtype()),
//       m_data(new std::byte[m_rows * m_cols * m_dtype.bytes()]) {

//     std::memcpy(m_data, other.m_data, m_rows * m_cols * m_dtype.bytes());
// }

Frame Frame::copy() const {
    Frame frame(m_rows, m_cols, m_dtype);
    std::memcpy(frame.m_data, m_data, m_rows * m_cols * m_dtype.bytes());
    return frame;
}

Frame::~Frame() noexcept { delete[] m_data; }
} // namespace aare
