#include "aare/Frame.hpp"
#include <iostream>

template <typename DataType>
Frame<DataType>::Frame(std::byte* bytes, ssize_t rows, ssize_t cols):
  m_rows(rows), m_cols(cols) {
    m_data = new DataType[rows*cols];
    std::memcpy(m_data, bytes, m_rows*m_cols*sizeof(DataType));
  }

template <typename DataType>
Frame<DataType>::Frame(ssize_t rows, ssize_t cols):
  m_rows(rows), m_cols(cols) {
    m_data = new DataType[m_rows*m_cols];
  }



template <typename DataType>
DataType Frame<DataType>::get(int row, int col) {
  if (row < 0 || row >= m_rows || col < 0 || col >= m_cols) {
    std::cerr << "Invalid row or column index" << std::endl;
    return 0;
  }
  return m_data[row*m_cols + col];
}


template class Frame<uint16_t>;
