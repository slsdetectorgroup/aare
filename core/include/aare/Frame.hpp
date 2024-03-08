#pragma once
#include "aare/defs.hpp"
#include <bits/unique_ptr.h>
#include <cstddef>
#include <cstdint>
#include <sys/types.h>
#include <vector>

/**
 * @brief Frame class to represent a single frame of data
 * model class
 * should be able to work with streams coming from files or network
 */

template <class DataType> class Frame {
    ssize_t m_rows;
    ssize_t m_cols;
    DataType *m_data;
    ssize_t m_bitdepth = sizeof(DataType) * 8;

    public:
    ssize_t rows;
    ssize_t cols;
    DataType* data;
    ssize_t bitdepth = sizeof(DataType)*8;
    Frame(ssize_t rows, ssize_t cols);

    Frame(std::byte *fp, ssize_t rows, ssize_t cols);
    DataType get(int row, int col);
    ssize_t rows() const{
        return m_rows;
    }
    ssize_t cols() const{
        return m_cols;
    }
    ssize_t bitdepth() const{
        return m_bitdepth;
    }

    ~Frame() { delete[] m_data; }
};

typedef Frame<uint16_t> Frame16;
typedef Frame<uint8_t> Frame8;
typedef Frame<uint32_t> Frame32;