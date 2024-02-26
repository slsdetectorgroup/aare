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
class IFrame {

    std::byte* data{nullptr};
    ssize_t rows{};
    ssize_t cols{};
    ssize_t bitdepth{};
    public:
    IFrame(std::byte* fp, ssize_t rows, ssize_t cols, ssize_t bitdepth);
    std::byte* get(int row, int col);
    ~IFrame(){
        delete[] data;
    }
};

template <class DataType> class Frame: public IFrame {
    public:
    Frame(std::byte* fp, ssize_t rows, ssize_t cols):IFrame(fp, rows, cols, sizeof(DataType)){}
    DataType get(int row, int col){
        return *((DataType*) IFrame::get(row, col));
    }
};