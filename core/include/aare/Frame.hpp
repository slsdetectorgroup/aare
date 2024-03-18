#pragma once
#include "aare/defs.hpp"
#include <bits/unique_ptr.h>
#include <cstddef>
#include <cstdint>
#include <vector>
#include "aare/IFrame.hpp"

/**
 * @brief Frame class to represent a single frame of data
 * model class
 * should be able to work with streams coming from files or network
 */

template <class DataType> 
class Frame: public IFrame<DataType> {
  public:
    Frame(ssize_t rows, ssize_t cols);
    Frame(std::byte *fp, ssize_t rows, ssize_t cols);
    ~Frame() { delete[] this->m_data; }
};

typedef Frame<uint16_t> Frame16;
typedef Frame<uint8_t> Frame8;
typedef Frame<uint32_t> Frame32;