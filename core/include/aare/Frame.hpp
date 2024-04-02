#pragma once
#include "aare/NDArray.hpp"
#include "aare/defs.hpp"
#include <cstddef>
#include <cstdint>
#include <memory>
#include <vector>
#include <sys/types.h>
#include <vector>

/**
 * @brief Frame class to represent a single frame of data
 * model class
 * should be able to work with streams coming from files or network
 */

namespace aare {

class Frame {
    ssize_t m_rows;
    ssize_t m_cols;
    ssize_t m_bitdepth;
    std::byte *m_data;

  public:
    Frame(ssize_t rows, ssize_t cols, ssize_t m_bitdepth);
    Frame(std::byte *fp, ssize_t rows, ssize_t cols, ssize_t m_bitdepth);
    std::byte *get(int row, int col);
    template <typename T> void set(int row, int col, T data);
    // std::vector<std::vector<DataType>> get_array();
    ssize_t rows() const { return m_rows; }
    ssize_t cols() const { return m_cols; }
    ssize_t bitdepth() const { return m_bitdepth; }
    inline ssize_t size() const { return m_rows * m_cols * m_bitdepth / 8; }
    std::byte *_get_data() { return m_data; }
    Frame &operator=(Frame &other) {
        m_rows = other.rows();
        m_cols = other.cols();
        m_bitdepth = other.bitdepth();
        m_data = new std::byte[m_rows * m_cols * m_bitdepth / 8];
        std::memcpy(m_data, other.m_data, m_rows * m_cols * m_bitdepth / 8);
        return *this;
    }
    // add move constructor
    Frame(Frame &&other) {
        m_rows = other.rows();
        m_cols = other.cols();
        m_bitdepth = other.bitdepth();
        m_data = other.m_data;
        other.m_data = nullptr;
        other.m_rows = other.m_cols = other.m_bitdepth = 0;
    }

    template <typename T> 
    NDView<T> view() {
        std::vector<ssize_t> shape = {m_rows, m_cols};
        T* data = reinterpret_cast<T *>(m_data);
        return NDView<T>(data, shape);
    }

    template <typename T>
    NDArray<T> image() {
        return NDArray<T>(this->view<T>());
    }

    ~Frame() { delete[] m_data; }
};


} // namespace aare