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
template <class DataType>
class Frame {

    DataType* data{nullptr};
    ssize_t rows{};
    ssize_t cols{};
    public:
    Frame(std::byte* fp, ssize_t rows, ssize_t cols);
    DataType get(int row, int col);
    ~Frame(){
        delete[] data;
    }
};