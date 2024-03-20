#pragma once
#include "aare/defs.hpp"
#include <memory>
#include <cstddef>
#include <cstdint>
#include <sys/types.h>
#include <vector>

/**
 * @brief Frame class to represent a single frame of data
 * model class
 * should be able to work with streams coming from files or network
 */

class Frame {
    ssize_t m_rows;
    ssize_t m_cols;
    ssize_t m_bitdepth;
    std::byte* m_data;


  public:
    Frame(ssize_t rows, ssize_t cols,ssize_t m_bitdepth);
    Frame(std::byte *fp, ssize_t rows, ssize_t cols,ssize_t m_bitdepth);
    std::byte* get(int row, int col);
    template <typename T>
    void set(int row, int col,T data);
    // std::vector<std::vector<DataType>> get_array();
    ssize_t rows() const{
        return m_rows;
    }
    ssize_t cols() const{
        return m_cols;
    }
    ssize_t bitdepth() const{
        return m_bitdepth;
    }
    std::byte* _get_data(){
        return m_data;
    }
    Frame& operator=(Frame& other){
            m_rows = other.rows();
            m_cols = other.cols();
            m_bitdepth = other.bitdepth();
            m_data = new std::byte[m_rows*m_cols*m_bitdepth/8];
            std::memcpy(m_data, other.m_data, m_rows*m_cols*m_bitdepth/8);
        return *this;
    }

    ~Frame() { delete[] m_data; }
};
