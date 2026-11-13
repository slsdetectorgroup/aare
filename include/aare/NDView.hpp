// SPDX-License-Identifier: MPL-2.0
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
#include <type_traits>
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
template <size_t Ndim>
Shape<Ndim - 1> drop_first_dim(const Shape<Ndim> &shape) {
    static_assert(Ndim > 1, "Cannot drop first dimension from a 1D shape");
    Shape<Ndim - 1> new_shape;
    std::copy(shape.begin() + 1, shape.end(), new_shape.begin());
    return new_shape;
}

/**
 * @brief Helper function when constructing NDArray/NDView. Calculates the
 * number of elements in the resulting array from a shape.
 * @param shape The shape to calculate the number of elements for.
 * @return The number of elements in and NDArray/NDView of that shape.
 */
template <size_t Ndim> size_t num_elements(const Shape<Ndim> &shape) {
    return std::accumulate(shape.begin(), shape.end(), size_t{1},
                           std::multiplies<size_t>());
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
class NDView : public NDOps<NDView<T, Ndim>, T, Ndim> {
    using Ops = NDOps<NDView<T, Ndim>, T, Ndim>;

  public:
    NDView() = default;
    ~NDView() = default;
    NDView(const NDView &) = default;
    NDView(NDView &&) = default;

    NDView(T *buffer, std::array<ssize_t, Ndim> shape)
        : buffer_(buffer), strides_(c_strides<Ndim>(shape)), shape_(shape),
          size_(num_elements(shape)) {}

    template <typename U, std::enable_if_t<
                              std::is_convertible_v<U (*)[], T (*)[]>, int> = 0>
    NDView(const NDView<U, Ndim> &other) noexcept
        : buffer_(other.buffer_), strides_(other.strides_),
          shape_(other.shape_), size_(other.size_) {}

    // view(i, j, ...) and view[i], shared with NDArray
    using Ops::operator();
    using Ops::operator[];

    // Not variadic: a template with the same parameter list as the inherited
    // operator()(Ix...) would hide it despite the using declaration.
    template <ssize_t D = Ndim>
    std::enable_if_t<(D > 1) && D == Ndim, NDView<T, D - 1>>
    operator()(ssize_t index) {
        // return a view of the next dimension
        return NDView<T, D - 1>(buffer_ + index * strides_[0],
                                drop_first_dim(shape_));
    }

    ssize_t size() const { return static_cast<ssize_t>(size_); }
    size_t total_bytes() const { return size_ * sizeof(T); }
    const std::array<ssize_t, Ndim> &strides() const noexcept {
        return strides_;
    }

    T *begin() { return buffer_; }
    T *end() { return buffer_ + size_; }
    T const *begin() const { return buffer_; }
    T const *end() const { return buffer_ + size_; }

    template <size_t Size> NDView &operator=(const std::array<T, Size> &arr) {
        if (size() != static_cast<ssize_t>(arr.size()))
            throw std::runtime_error(LOCATION +
                                     "Array and NDView size mismatch");
        std::copy(arr.begin(), arr.end(), begin());
        return *this;
    }

    NDView &operator=(const T val) {
        std::fill(begin(), end(), val);
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

    /**
     * @brief Create a subview of a single index of the first dimension.
     * This is useful for accessing a single frame in a batch of frames.
     * @param index The index of the subview.
     * @return A new NDView that is a subview of the current view.
     * @throws std::runtime_error if the index is invalid.
     */
    template <int D = Ndim, std::enable_if_t<(D > 1) && D == Ndim, int> = 0>
    NDView<T, D - 1> sub_view(const ssize_t index) const {
        if (index < 0 || index >= shape_[0])
            throw std::runtime_error(LOCATION + "Invalid sub_view index");
        auto new_shape = drop_first_dim(shape_);
        return NDView<T, D - 1>(buffer_ + index * strides_[0], new_shape);
    }

  private:
    template <typename, ssize_t> friend class NDView;

    T *buffer_{nullptr};
    std::array<ssize_t, Ndim> strides_{};
    std::array<ssize_t, Ndim> shape_{};
    uint64_t size_{};
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

/** @brief Print a 2D NDArray or NDView row by row. */
template <typename Derived, typename T, ssize_t Ndim>
std::ostream &operator<<(std::ostream &os, const NDOps<Derived, T, Ndim> &ops) {
    const auto &arr = static_cast<const Derived &>(ops);
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
