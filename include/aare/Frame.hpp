#pragma once
#include "aare/Dtype.hpp"
#include "aare/NDArray.hpp"
#include "aare/defs.hpp"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <vector>

namespace aare {

/**
 * @brief Frame class to represent a single frame of data. Not much more than a
 * pointer and some info. Limited interface to accept frames from many sources.
 */
class Frame {
    uint32_t m_rows;
    uint32_t m_cols;
    Dtype m_dtype;
    std::byte *m_data;
    //TODO! Add frame number?

  public:
    /**
     * @brief Construct a new Frame
     * @param rows number of rows
     * @param cols number of columns
     * @param dtype data type of the pixels
     * @note the data is initialized to zero
     */
    Frame(uint32_t rows, uint32_t cols, Dtype dtype);

    /**
     * @brief Construct a new Frame
     * @param bytes pointer to the data to be copied into the frame
     * @param rows number of rows
     * @param cols number of columns
     * @param dtype data type of the pixels
     */
    Frame(const std::byte *bytes, uint32_t rows, uint32_t cols, Dtype dtype);
    ~Frame(){ delete[] m_data; };

    /** @warning Copy is disabled to ensure performance when passing
     * frames around. Can discuss enabling it.
     *
     */
    Frame &operator=(const Frame &other) = delete;
    Frame(const Frame &other) = delete;

    // enable move
    Frame &operator=(Frame &&other) noexcept;
    Frame(Frame &&other) noexcept;


    Frame clone() const; //<- Explicit copy

    uint32_t rows() const;
    uint32_t cols() const;
    size_t bitdepth() const;
    Dtype dtype() const;
    uint64_t size() const;
    size_t bytes() const;
    std::byte *data() const;

    /**
     * @brief Get the pointer to the pixel at the given row and column
     * @param row row index
     * @param col column index
     * @return pointer to the pixel
     * @warning The user should cast the pointer to the appropriate type. Think
     * twice if this is the function you want to use.
     */
    std::byte *pixel_ptr(uint32_t row, uint32_t col) const;

    /**
     * @brief Set the pixel at the given row and column to the given value
     * @tparam T type of the value
     * @param row row index
     * @param col column index
     * @param data value to set
     */
    template <typename T> void set(uint32_t row, uint32_t col, T data) {
        assert(sizeof(T) == m_dtype.bytes());
        if (row >= m_rows || col >= m_cols) {
            throw std::out_of_range("Invalid row or column index");
        }
        std::memcpy(m_data + (row * m_cols + col) * m_dtype.bytes(), &data,
                    m_dtype.bytes());
    }
    template <typename T> T get(uint32_t row, uint32_t col) {
        assert(sizeof(T) == m_dtype.bytes());
        if (row >= m_rows || col >= m_cols) {
            throw std::out_of_range("Invalid row or column index");
        }
        //TODO! add tests then reimplement using pixel_ptr
        T data;
        std::memcpy(&data, m_data + (row * m_cols + col) * m_dtype.bytes(),
                    m_dtype.bytes());
        return data;
    }
    /**
     * @brief Return an NDView of the frame. This is the preferred way to access
     * data in the frame.
     * 
     * @tparam T type of the pixels
     * @return NDView<T, 2> 
     */
    template <typename T> NDView<T, 2> view() {
        std::array<int64_t, 2> shape = {static_cast<int64_t>(m_rows),
                                        static_cast<int64_t>(m_cols)};
        T *data = reinterpret_cast<T *>(m_data);
        return NDView<T, 2>(data, shape);
    }

    /** 
     * @brief Copy the frame data into a new NDArray. This is a deep copy.
     */
    template <typename T> NDArray<T> image() {
        return NDArray<T>(this->view<T>());
    }
};

} // namespace aare