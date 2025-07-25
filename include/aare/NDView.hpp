#pragma once
#include "aare/ArrayExpr.hpp"
#include "aare/defs.hpp"

#include <algorithm>
#include <array>
#include <cassert>
#include <cstdint>
#include <functional>
#include <iomanip>
#include <iostream>
#include <numeric>
#include <stdexcept>
#include <vector>
namespace aare {

template <ssize_t Ndim> using Shape = std::array<ssize_t, Ndim>;

// TODO! fix mismatch between signed and unsigned
template <ssize_t Ndim>
Shape<Ndim> make_shape(const std::vector<size_t> &shape) {
    if (shape.size() != Ndim)
        throw std::runtime_error("Shape size mismatch");
    Shape<Ndim> arr;
    std::copy_n(shape.begin(), Ndim, arr.begin());
    return arr;
}


/**
 * @brief Helper function to drop the first dimension of a shape.
 * This is useful when you want to create a 2D view from a 3D array.
 * @param shape The shape to drop the first dimension from.
 * @return A new shape with the first dimension dropped.
 */
template<size_t Ndim>
Shape<Ndim-1> drop_first_dim(const Shape<Ndim> &shape) {
    static_assert(Ndim > 1, "Cannot drop first dimension from a 1D shape");
    Shape<Ndim - 1> new_shape;
    std::copy(shape.begin() + 1, shape.end(), new_shape.begin());
    return new_shape;
}

/**
 * @brief Helper function when constructing NDArray/NDView. Calculates the number
 * of elements in the resulting array from a shape.
 * @param shape The shape to calculate the number of elements for.
 * @return The number of elements in and NDArray/NDView of that shape.
 */
template <size_t Ndim>
size_t num_elements(const Shape<Ndim> &shape) {
    return std::accumulate(shape.begin(), shape.end(), 1,
                           std::multiplies<size_t>());
}

template <ssize_t Dim = 0, typename Strides>
ssize_t element_offset(const Strides & /*unused*/) {
    return 0;
}

template <ssize_t Dim = 0, typename Strides, typename... Ix>
ssize_t element_offset(const Strides &strides, ssize_t i, Ix... index) {
    return i * strides[Dim] + element_offset<Dim + 1>(strides, index...);
}

template <ssize_t Ndim>
std::array<ssize_t, Ndim> c_strides(const std::array<ssize_t, Ndim> &shape) {
    std::array<ssize_t, Ndim> strides{};
    std::fill(strides.begin(), strides.end(), 1);
    for (ssize_t i = Ndim - 1; i > 0; --i) {
        strides[i - 1] = strides[i] * shape[i];
    }
    return strides;
}

template <ssize_t Ndim>
std::array<ssize_t, Ndim> make_array(const std::vector<ssize_t> &vec) {
    assert(vec.size() == Ndim);
    std::array<ssize_t, Ndim> arr{};
    std::copy_n(vec.begin(), Ndim, arr.begin());
    return arr;
}

template <typename T, ssize_t Ndim = 2>
class NDView : public ArrayExpr<NDView<T, Ndim>, Ndim> {
  public:
    NDView() = default;
    ~NDView() = default;
    NDView(const NDView &) = default;
    NDView(NDView &&) = default;

    NDView(T *buffer, std::array<ssize_t, Ndim> shape)
        : buffer_(buffer), strides_(c_strides<Ndim>(shape)), shape_(shape),
          size_(std::accumulate(std::begin(shape), std::end(shape), 1,
                                std::multiplies<>())) {}
                   
    template <typename... Ix>
    std::enable_if_t<sizeof...(Ix) == Ndim, T &> operator()(Ix... index) {
        return buffer_[element_offset(strides_, index...)];
    }

    template <typename... Ix>
    std::enable_if_t<sizeof...(Ix) == 1 && (Ndim > 1), NDView<T, Ndim - 1>> operator()(Ix... index) {
        // return a view of the next dimension
        std::array<ssize_t, Ndim - 1> new_shape{};
        std::copy_n(shape_.begin() + 1, Ndim - 1, new_shape.begin());
        return NDView<T, Ndim - 1>(&buffer_[element_offset(strides_, index...)],
                                   new_shape);
        
    }

    template <typename... Ix>
    std::enable_if_t<sizeof...(Ix) == Ndim, const T &> operator()(Ix... index) const {
        return buffer_[element_offset(strides_, index...)];
    }


    ssize_t size() const { return static_cast<ssize_t>(size_); }
    size_t total_bytes() const { return size_ * sizeof(T); }
    std::array<ssize_t, Ndim> strides() const noexcept { return strides_; }

    T *begin() { return buffer_; }
    T *end() { return buffer_ + size_; }
    T const *begin() const { return buffer_; }
    T const *end() const { return buffer_ + size_; }
    
    



    /**
     * @brief Access element at index i.
     */
    T &operator[](ssize_t i)  { return buffer_[i]; }

    /**
     * @brief Access element at index i.
     */
    const T &operator[](ssize_t i) const { return buffer_[i]; }

    bool operator==(const NDView &other) const {
        if (size_ != other.size_)
            return false;
        if (shape_ != other.shape_)
            return false;
        for (size_t i = 0; i != size_; ++i) {
            if (buffer_[i] != other.buffer_[i])
                return false;
        }
        return true;
    }

    NDView &operator+=(const T val) { return elemenwise(val, std::plus<T>()); }
    NDView &operator-=(const T val) { return elemenwise(val, std::minus<T>()); }
    NDView &operator*=(const T val) {
        return elemenwise(val, std::multiplies<T>());
    }
    NDView &operator/=(const T val) {
        return elemenwise(val, std::divides<T>());
    }

    NDView &operator/=(const NDView &other) {
        return elemenwise(other, std::divides<T>());
    }

    template <size_t Size> NDView &operator=(const std::array<T, Size> &arr) {
        if (size() != static_cast<ssize_t>(arr.size()))
            throw std::runtime_error(LOCATION +
                                     "Array and NDView size mismatch");
        std::copy(arr.begin(), arr.end(), begin());
        return *this;
    }

    NDView &operator=(const T val) {
        for (auto it = begin(); it != end(); ++it)
            *it = val;
        return *this;
    }

    NDView &operator=(const NDView &other) {
        if (this == &other)
            return *this;
        shape_ = other.shape_;
        strides_ = other.strides_;
        size_ = other.size_;
        buffer_ = other.buffer_;
        return *this;
    }

    NDView &operator=(NDView &&other) noexcept {
        if (this == &other)
            return *this;
        shape_ = std::move(other.shape_);
        strides_ = std::move(other.strides_);
        size_ = other.size_;
        buffer_ = other.buffer_;
        other.buffer_ = nullptr;
        return *this;
    }

    auto &shape() const { return shape_; }
    auto shape(ssize_t i) const { return shape_[i]; }

    T *data() { return buffer_; }
    const T *data() const { return buffer_; }
    void print_all() const;

    /**
     * @brief Create a subview of a range of the first dimension. 
     * This is useful for splitting a batches of frames in parallel processing.
     * @param first The first index of the subview (inclusive).
     * @param last The last index of the subview (exclusive).
     * @return A new NDView that is a subview of the current view.
     * @throws std::runtime_error if the range is invalid.
     */
    NDView sub_view(ssize_t first, ssize_t last) const {
        if (first < 0 || last > shape_[0] || first >= last)
            throw std::runtime_error(LOCATION + "Invalid sub_view range");
        auto new_shape = shape_;
        new_shape[0] = last - first;
        return NDView(buffer_ + first * strides_[0], new_shape);
    }

  private:
    T *buffer_{nullptr};
    std::array<ssize_t, Ndim> strides_{};
    std::array<ssize_t, Ndim> shape_{};
    uint64_t size_{};

    template <class BinaryOperation>
    NDView &elemenwise(T val, BinaryOperation op) {
        for (uint64_t i = 0; i != size_; ++i) {
            buffer_[i] = op(buffer_[i], val);
        }
        return *this;
    }
    template <class BinaryOperation>
    NDView &elemenwise(const NDView &other, BinaryOperation op) {
        for (uint64_t i = 0; i != size_; ++i) {
            buffer_[i] = op(buffer_[i], other.buffer_[i]);
        }
        return *this;
    }
};

template <typename T, ssize_t Ndim> void NDView<T, Ndim>::print_all() const {
    for (auto row = 0; row < shape_[0]; ++row) {
        for (auto col = 0; col < shape_[1]; ++col) {
            std::cout << std::setw(3);
            std::cout << (*this)(row, col) << " ";
        }
        std::cout << "\n";
    }
}

template <typename T, ssize_t Ndim>
std::ostream &operator<<(std::ostream &os, const NDView<T, Ndim> &arr) {
    for (auto row = 0; row < arr.shape(0); ++row) {
        for (auto col = 0; col < arr.shape(1); ++col) {
            os << std::setw(3);
            os << arr(row, col) << " ";
        }
        os << "\n";
    }
    return os;
}

template <typename T> NDView<T, 1> make_view(std::vector<T> &vec) {
    return NDView<T, 1>(vec.data(), {static_cast<ssize_t>(vec.size())});
}

} // namespace aare