#pragma once
#include "aare/core/NDArray.hpp"
#include "aare/core/defs.hpp"
#include "aare/core/Dtype.hpp"
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
    Dtype m_dtype;
    std::byte *m_data;

  public:
    Frame(size_t rows, size_t cols, Dtype dtype);
    Frame(std::byte *bytes, size_t rows, size_t cols, Dtype dtype);
    std::byte *get(size_t row, size_t col);

    // TODO! can we, or even want to remove the template?
    template <typename T> void set(size_t row, size_t col, T data) {
        assert(sizeof(T) == m_dtype.bytes());
        if (row >= m_rows || col >= m_cols) {
            throw std::out_of_range("Invalid row or column index");
        }
        std::memcpy(m_data + (row * m_cols + col) * m_dtype.bytes(), &data, m_dtype.bytes());
    }

    size_t rows() const { return m_rows; }
    size_t cols() const { return m_cols; }
    size_t bitdepth() const { return m_dtype.bitdepth(); }
    Dtype dtype() const { return m_dtype; }
    size_t size() const { return m_rows * m_cols * m_dtype.bytes(); }
    std::byte *data() const { return m_data; }

    Frame &operator=(const Frame &other) {
        if (this == &other) {
            return *this;
        }
        m_rows = other.rows();
        m_cols = other.cols();
        m_dtype = other.dtype();
        m_data = new std::byte[m_rows * m_cols * m_dtype.bytes()];
        if (m_data == nullptr) {
            throw std::bad_alloc();
        }
        std::memcpy(m_data, other.m_data, m_rows * m_cols * m_dtype.bytes());
        return *this;
    }

    Frame &operator=(Frame &&other) noexcept {
        if (this == &other) {
            return *this;
        }
        m_rows = other.rows();
        m_cols = other.cols();
        m_dtype = other.dtype();
        if (m_data != nullptr) {
            delete[] m_data;
        }
        m_data = other.m_data;
        other.m_data = nullptr;
        other.m_rows = other.m_cols  = 0;
        other.m_dtype = Dtype(Dtype::TypeIndex::ERROR);
        return *this;
    }

    // add move constructor
    Frame(Frame &&other) noexcept
        : m_rows(other.rows()), m_cols(other.cols()), m_dtype(other.dtype()), m_data(other.m_data) {

        other.m_data = nullptr;
        other.m_rows = other.m_cols  = 0;
        other.m_dtype = Dtype(Dtype::TypeIndex::ERROR);
    }
    // copy constructor
    Frame(const Frame &other)
        : m_rows(other.rows()), m_cols(other.cols()),m_dtype(other.dtype()),
          m_data(new std::byte[m_rows * m_cols * m_dtype.bytes()]) {

        std::memcpy(m_data, other.m_data, m_rows * m_cols * m_dtype.bytes());
    }

    template <typename T> NDView<T, 2> view() {
        std::array<int64_t, 2> shape = {static_cast<int64_t>(m_rows), static_cast<int64_t>(m_cols)};
        T *data = reinterpret_cast<T *>(m_data);
        return NDView<T, 2>(data, shape);
    }

    template <typename T> NDArray<T> image() { return NDArray<T>(this->view<T>()); }

    ~Frame() { delete[] m_data; }
};

} // namespace aare