#include "aare/Frame.hpp"
#include <iostream>

template <typename DataType>
Frame<DataType>::Frame(std::byte* bytes, ssize_t rows, ssize_t cols):
  rows(rows), cols(cols) {
    data = new DataType[rows*cols];
    std::memcpy(data, bytes, rows*cols*sizeof(DataType));
  }

template <typename DataType>
Frame<DataType>::Frame(ssize_t rows, ssize_t cols):
  rows(rows), cols(cols) {
    data = new DataType[rows*cols];
  }



template <typename DataType>
DataType Frame<DataType>::get(int row, int col) {
  if (row < 0 || row >= rows || col < 0 || col >= cols) {
    throw std::runtime_error("Invalid row or column index");
  }
  return data[row*cols + col];
}


template class Frame<uint16_t>;
