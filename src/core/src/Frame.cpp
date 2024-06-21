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
Frame::Frame(std::byte *bytes, size_t rows, size_t cols, size_t bitdepth)
    : m_rows(rows), m_cols(cols), m_bitdepth(bitdepth), m_data(new std::byte[rows * cols * bitdepth / 8]) {

    std::memcpy(m_data, bytes, rows * cols * bitdepth / 8);
}

/**
 * @brief Construct a new Frame
 * @param rows number of rows
 * @param cols number of columns
 * @param bitdepth bitdepth of the pixels
 * @note the data is initialized to zero
 */
Frame::Frame(size_t rows, size_t cols, size_t bitdepth)
    : m_rows(rows), m_cols(cols), m_bitdepth(bitdepth), m_data(new std::byte[rows * cols * bitdepth / 8]) {

    std::memset(m_data, 0, rows * cols * bitdepth / 8);
}

/**
 * @brief Get the pointer to the pixel at the given row and column
 * @param row row index
 * @param col column index
 * @return pointer to the pixel
 * @note the user should cast the pointer to the appropriate type
 */
std::byte *Frame::get(size_t row, size_t col) {
    if ((row >= m_rows) || (col >= m_cols)) {
        std::cerr << "Invalid row or column index" << '\n';
        return nullptr;
    }
    return m_data + (row * m_cols + col) * (m_bitdepth / 8);
}

} // namespace aare
