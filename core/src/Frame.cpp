#include "aare/core/Frame.hpp"
#include "aare/utils/logger.hpp"
#include <cassert>
#include <iostream>

namespace aare {

Frame::Frame(std::byte *bytes, ssize_t rows, ssize_t cols, ssize_t bitdepth)
    : m_rows(rows), m_cols(cols), m_bitdepth(bitdepth) {
    m_data = new std::byte[rows * cols * bitdepth / 8];
    std::memcpy(m_data, bytes, rows * cols * bitdepth / 8);
}

Frame::Frame(ssize_t rows, ssize_t cols, ssize_t bitdepth) : m_rows(rows), m_cols(cols), m_bitdepth(bitdepth) {
    m_data = new std::byte[rows * cols * bitdepth / 8];
    std::memset(m_data, 0, rows * cols * bitdepth / 8);
}

std::byte *Frame::get(int row, int col) {
    if (row < 0 || row >= m_rows || col < 0 || col >= m_cols) {
        std::cerr << "Invalid row or column index" << std::endl;
        return 0;
    }
    return m_data + (row * m_cols + col) * (m_bitdepth / 8);
}

} // namespace aare
