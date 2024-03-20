#include "aare/Frame.hpp"
#include <iostream>
#include <cassert>
Frame::Frame(std::byte* bytes, ssize_t rows, ssize_t cols, ssize_t bitdepth):
  m_rows(rows), m_cols(cols), m_bitdepth(bitdepth) {
    m_data = new std::byte[rows*cols*bitdepth/8];
    std::memcpy(m_data, bytes, rows*cols*bitdepth/8);
  }

Frame::Frame(ssize_t rows, ssize_t cols, ssize_t bitdepth):
  m_rows(rows), m_cols(cols), m_bitdepth(bitdepth) {
    m_data = new std::byte[rows*cols*bitdepth/8];
  }



std::byte* Frame::get(int row, int col) {
  if (row < 0 || row >= m_rows || col < 0 || col >= m_cols) {
    std::cerr << "Invalid row or column index" << std::endl;
    return 0;
  }
  return m_data+(row*m_cols + col)*(m_bitdepth/8);
}

template <typename T>
void Frame::set(int row, int col, T data) {
  assert(sizeof(T) == m_bitdepth/8);
  if (row < 0 || row >= m_rows || col < 0 || col >= m_cols) {
    std::cerr << "Invalid row or column index" << std::endl;
    return;
  }
  std::memcpy(m_data+(row*m_cols + col)*(m_bitdepth/8), &data, m_bitdepth/8);
}

template void Frame::set(int row, int col, uint16_t data);


// std::vector<std::vector<DataType>> Frame<DataType>::get_array() {
//   std::vector<std::vector<DataType>> array;
//   for (int i = 0; i < m_rows; i++) {
//     std::vector<DataType> row;
//     row.assign(m_data + i*m_cols, m_data + (i+1)*m_cols);
//     array.push_back(row);
//   }

//   return array;
// }


