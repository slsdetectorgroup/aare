#pragma once
#include "aare/core/NDArray.hpp"
#include "aare/core/defs.hpp"
#include <cstddef>
#include <cstdint>
#include <memory>
#include <sys/types.h>
#include <vector>

namespace aare {

/**
 * @brief Frame class to represent a single frame of data
 * model class
 * should be able to work with streams coming from files or network
 */
class Frame {
    ssize_t m_rows;
    ssize_t m_cols;
    ssize_t m_bitdepth;
    std::byte *m_data;

  public:
    Frame(ssize_t rows, ssize_t cols, ssize_t m_bitdepth);
    Frame(std::byte *fp, ssize_t rows, ssize_t cols, ssize_t m_bitdepth);
    std::byte *get(int row, int col);

    // TODO! can we, or even want to remove the template?
    template <typename T> void set(int row, int col, T data) {
        assert(sizeof(T) == m_bitdepth / 8);
        if (row < 0 || row >= m_rows || col < 0 || col >= m_cols) {
            throw std::out_of_range("Invalid row or column index");
        }
        std::memcpy(m_data + (row * m_cols + col) * (m_bitdepth / 8), &data, m_bitdepth / 8);
    }

    ssize_t rows() const { return m_rows; }
    ssize_t cols() const { return m_cols; }
    ssize_t bitdepth() const { return m_bitdepth; }
    ssize_t size() const { return m_rows * m_cols * m_bitdepth / 8; }
    std::byte *data() const { return m_data; }

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
    // copy constructor
    Frame(const Frame &other) {
        m_rows = other.rows();
        m_cols = other.cols();
        m_bitdepth = other.bitdepth();
        m_data = new std::byte[m_rows * m_cols * m_bitdepth / 8];
        std::memcpy(m_data, other.m_data, m_rows * m_cols * m_bitdepth / 8);
    }

    template <typename T> NDView<T> view() {
        std::vector<ssize_t> shape = {m_rows, m_cols};
        T *data = reinterpret_cast<T *>(m_data);
        return NDView<T>(data, shape);
    }

    template <typename T> NDArray<T> image() { return NDArray<T>(this->view<T>()); }

    ~Frame() { delete[] m_data; }
};

} // namespace aare