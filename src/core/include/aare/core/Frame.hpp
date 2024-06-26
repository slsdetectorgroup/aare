#pragma once
#include "aare/core/Dtype.hpp"
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
    Dtype m_dtype;
    std::byte *m_data;

  public:
    Frame(size_t rows, size_t cols, Dtype dtype);
    Frame(std::byte *bytes, size_t rows, size_t cols, Dtype dtype);
    ~Frame() noexcept;
    Frame &operator=(const Frame &other);
    Frame &operator=(Frame &&other) noexcept;
    Frame(Frame &&other) noexcept;
    Frame(const Frame &other);

    size_t rows() const;
    size_t cols() const;
    size_t bitdepth() const;
    Dtype dtype() const;
    size_t size() const;
    std::byte *data() const;

    std::byte *get(size_t row, size_t col);

    // TODO! can we, or even want to remove the template?
    template <typename T> void set(size_t row, size_t col, T data) {
        assert(sizeof(T) == m_dtype.bytes());
        if (row >= m_rows || col >= m_cols) {
            throw std::out_of_range("Invalid row or column index");
        }
        std::memcpy(m_data + (row * m_cols + col) * m_dtype.bytes(), &data, m_dtype.bytes());
    }
    template <typename T> T get_t(size_t row, size_t col) {
        assert(sizeof(T) == m_dtype.bytes());
        if (row >= m_rows || col >= m_cols) {
            throw std::out_of_range("Invalid row or column index");
        }
        T data;
        std::memcpy(&data, m_data + (row * m_cols + col) * m_dtype.bytes(), m_dtype.bytes());
        return data;
    }
    template <typename T> NDView<T, 2> view() {
        std::array<int64_t, 2> shape = {static_cast<int64_t>(m_rows), static_cast<int64_t>(m_cols)};
        T *data = reinterpret_cast<T *>(m_data);
        return NDView<T, 2>(data, shape);
    }

    template <typename T> NDArray<T> image() { return NDArray<T>(this->view<T>()); }
};

} // namespace aare