#pragma once
#include <cstddef>
#include <sys/types.h>
#include <cstdint>
#include <bits/unique_ptr.h>
#include <vector>
#include "common/defs.hpp"



/**
 * @brief Frame class to represent a single frame of data
 * model class
 * should be able to work with streams coming from files or network
*/


template <class DataType> class Frame{

    public:
    ssize_t rows;
    ssize_t cols;
    DataType* data;
    ssize_t bitdepth = sizeof(DataType)*8;

    Frame(std::byte* fp, ssize_t rows, ssize_t cols);
    DataType get(int row, int col);

    ~Frame(){
        delete[] data;
    }

};

typedef Frame<uint16_t> Frame16;
typedef Frame<uint8_t> Frame8;
typedef Frame<uint32_t> Frame32;