#include "Frame.hpp"
#include <iostream>


IFrame::IFrame(std::byte* bytes, ssize_t rows, ssize_t cols, ssize_t bitdepth)
 {
  this->rows = rows;
  this->cols = cols;
  data = new std::byte[rows * cols*bitdepth/8];
  std::memcpy(data, bytes, bitdepth/8 * rows * cols);
}

std::byte* IFrame::get(int row, int col) {
  if (row < 0 || row >= rows || col < 0 || col >= cols) {
    std::cerr << "Invalid row or column index" << std::endl;
    return 0;
  }
  return  data+(row * cols + col)*bitdepth/8;


}

