#include "aare/Frame.hpp"
#include <iostream>


template <class DataType>
Frame<DataType>::Frame(std::byte* bytes, ssize_t rows, ssize_t cols)
   {
    this->m_rows = rows;
    this->m_cols = cols;
    this->m_data = new DataType[rows*cols];
    std::memcpy(this->m_data, bytes, this->m_rows*this->m_cols*sizeof(DataType));
  }

template <class DataType>
Frame<DataType>::Frame(ssize_t rows, ssize_t cols)
   {
    this->m_rows = rows;
    this->m_cols = cols;
    this->m_data = new DataType[this->m_rows*this->m_cols];
  }







template class Frame<uint8_t>;
template class Frame<uint16_t>;
template class Frame<uint32_t>;
