#pragma once
#include "aare/core/NDArray.hpp"
#include "aare/core/defs.hpp"
#include <cstddef>
#include <cstdint>
#include <memory>
#include <vector>

namespace aare {

/**
 * @brief Frame class to represent a single frame of data
 * model class
 * should be able to work with streams coming from files or network
 */
class Frame {
    size_t m_rows;
    size_t m_cols;
    size_t m_bitdepth;
    std::byte *m_data;

  public:
    Frame(size_t rows, size_t cols, size_t m_bitdepth);
    Frame(std::byte *bytes, size_t rows, size_t cols, size_t m_bitdepth);
    std::byte *get(size_t row, size_t col);

    // TODO! can we, or even want to remove the template?
    template <typename T> void set(size_t row, size_t col, T data) {
        assert(sizeof(T) == m_bitdepth / 8);
        if (row >= m_rows or col >= m_cols) {
            throw std::out_of_range("Invalid row or column index");
        }
        std::memcpy(m_data + (row * m_cols + col) * (m_bitdepth / 8), &data, m_bitdepth / 8);
    }

    size_t rows() const { return m_rows; }
    size_t cols() const { return m_cols; }
    size_t bitdepth() const { return m_bitdepth; }
    size_t size() const { return m_rows * m_cols * m_bitdepth / 8; }
    std::byte *data() const { return m_data; }

    Frame &operator=(const Frame &other) {
        if (this == &other) {
            return *this;
        }
        m_rows = other.rows();
        m_cols = other.cols();
        m_bitdepth = other.bitdepth();
        m_data = new std::byte[m_rows * m_cols * m_bitdepth / 8];
        if (m_data == nullptr) {
            throw std::bad_alloc();
        }
        std::memcpy(m_data, other.m_data, m_rows * m_cols * m_bitdepth / 8);
        return *this;
    }

    Frame &operator=(Frame &&other) noexcept {
        m_rows = other.rows();
        m_cols = other.cols();
        m_bitdepth = other.bitdepth();
        m_data = other.m_data;
        other.m_data = nullptr;
        other.m_rows = other.m_cols = other.m_bitdepth = 0;
        return *this;
    }

    // add move constructor
    Frame(Frame &&other) noexcept
        : m_rows(other.rows()), m_cols(other.cols()), m_bitdepth(other.bitdepth()), m_data(other.m_data) {

        other.m_data = nullptr;
        other.m_rows = other.m_cols = other.m_bitdepth = 0;
    }
    // copy constructor
    Frame(const Frame &other)
        : m_rows(other.rows()), m_cols(other.cols()), m_bitdepth(other.bitdepth()),
          m_data(new std::byte[m_rows * m_cols * m_bitdepth / 8]) {

        std::memcpy(m_data, other.m_data, m_rows * m_cols * m_bitdepth / 8);
    }

    template <typename T> NDView<T, 2> view() {
        std::array<ssize_t, 2> shape = {static_cast<ssize_t>(m_rows), static_cast<ssize_t>(m_cols)};
        T *data = reinterpret_cast<T *>(m_data);
        return NDView<T, 2>(data, shape);
    }

    template <typename T> NDArray<T> image() { return NDArray<T>(this->view<T>()); }

    ~Frame() { delete[] m_data; }
};

} // namespace aare