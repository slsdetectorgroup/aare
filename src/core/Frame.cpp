#include "Frame.hpp"
#include <iostream>

template <typename DataType>
Frame<DataType>::Frame(std::byte* bytes, ssize_t rows, ssize_t cols):
  rows(rows), cols(cols) {
    data = new DataType[rows*cols];
    std::memcpy(data, bytes, rows*cols*sizeof(DataType));
  }



template <typename DataType>
DataType Frame<DataType>::get(int row, int col) {
  if (row < 0 || row >= rows || col < 0 || col >= cols) {
    std::cerr << "Invalid row or column index" << std::endl;
    return 0;
  }
  return data[row*cols + col];
}


template class Frame<uint16_t>;
