#pragma once
#include <cstddef>
#include <sys/types.h>
#include <cstdint>
#include <bits/unique_ptr.h>
#include <vector>
#include "defs.hpp"



/**
 * @brief Frame class to represent a single frame of data
 * model class
 * should be able to work with streams coming from files or network
*/
class FrameImpl {
    protected:
    std::byte* data{nullptr};
    ssize_t rows{};
    ssize_t cols{};
    ssize_t bitdepth{};
    public:
    FrameImpl(std::byte* fp, ssize_t rows, ssize_t cols, ssize_t bitdepth);
    std::byte* get(int row, int col);
    ~FrameImpl(){
        delete[] data;
    }
};

template <class DataType> class Frame: public FrameImpl {
    public:
    Frame(std::byte* fp, ssize_t rows, ssize_t cols):FrameImpl(fp, rows, cols, sizeof(DataType)){}
    DataType get(int row, int col){
        return *((DataType*) FrameImpl::get(row, col));
    }

};

typedef Frame<uint16_t> Frame16;
typedef Frame<uint8_t> Frame8;
typedef Frame<uint32_t> Frame32;