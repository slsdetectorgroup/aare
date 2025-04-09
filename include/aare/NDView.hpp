#pragma once
#include "aare/defs.hpp"
#include "aare/ArrayExpr.hpp"

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

template <int64_t Ndim> using Shape = std::array<int64_t, Ndim>;

// TODO! fix mismatch between signed and unsigned
template <int64_t Ndim> Shape<Ndim> make_shape(const std::vector<size_t> &shape) {
    if (shape.size() != Ndim)
        throw std::runtime_error("Shape size mismatch");
    Shape<Ndim> arr;
    std::copy_n(shape.begin(), Ndim, arr.begin());
    return arr;
}

template <int64_t Dim = 0, typename Strides> int64_t element_offset(const Strides & /*unused*/) { return 0; }

template <int64_t Dim = 0, typename Strides, typename... Ix>
int64_t element_offset(const Strides &strides, int64_t i, Ix... index) {
    return i * strides[Dim] + element_offset<Dim + 1>(strides, index...);
}

template <int64_t Ndim> std::array<int64_t, Ndim> c_strides(const std::array<int64_t, Ndim> &shape) {
    std::array<int64_t, Ndim> strides{};
    std::fill(strides.begin(), strides.end(), 1);
    for (int64_t i = Ndim - 1; i > 0; --i) {
        strides[i - 1] = strides[i] * shape[i];
    }
    return strides;
}

template <int64_t Ndim> std::array<int64_t, Ndim> make_array(const std::vector<int64_t> &vec) {
    assert(vec.size() == Ndim);
    std::array<int64_t, Ndim> arr{};
    std::copy_n(vec.begin(), Ndim, arr.begin());
    return arr;
}

template <typename T, int64_t Ndim = 2> class NDView : public ArrayExpr<NDView<T, Ndim>, Ndim> {
  public:
    NDView() = default;
    ~NDView() = default;
    NDView(const NDView &) = default;
    NDView(NDView &&) = default;

    NDView(T *buffer, std::array<int64_t, Ndim> shape)
        : buffer_(buffer), strides_(c_strides<Ndim>(shape)), shape_(shape),
          size_(std::accumulate(std::begin(shape), std::end(shape), 1, std::multiplies<>())) {}

    // NDView(T *buffer, const std::vector<int64_t> &shape)
    //     : buffer_(buffer), strides_(c_strides<Ndim>(make_array<Ndim>(shape))), shape_(make_array<Ndim>(shape)),
    //       size_(std::accumulate(std::begin(shape), std::end(shape), 1, std::multiplies<>())) {}

    template <typename... Ix> std::enable_if_t<sizeof...(Ix) == Ndim, T &> operator()(Ix... index) {
        return buffer_[element_offset(strides_, index...)];
    }

    template <typename... Ix> std::enable_if_t<sizeof...(Ix) == Ndim, T &> operator()(Ix... index) const {
        return buffer_[element_offset(strides_, index...)];
    }

    ssize_t size() const { return static_cast<ssize_t>(size_); }
    size_t total_bytes() const { return size_ * sizeof(T); }
    std::array<int64_t, Ndim> strides() const noexcept { return strides_; }

    T *begin() { return buffer_; }
    T *end() { return buffer_ + size_; }
    T const *begin() const { return buffer_; }
    T const *end() const { return buffer_ + size_; }
    T &operator()(int64_t i) const { return buffer_[i]; }
    T &operator[](int64_t i) const { return buffer_[i]; }

    bool operator==(const NDView &other) const {
        if (size_ != other.size_)
            return false;
        for (uint64_t i = 0; i != size_; ++i) {
            if (buffer_[i] != other.buffer_[i])
                return false;
        }
        return true;
    }

    NDView &operator+=(const T val) { return elemenwise(val, std::plus<T>()); }
    NDView &operator-=(const T val) { return elemenwise(val, std::minus<T>()); }
    NDView &operator*=(const T val) { return elemenwise(val, std::multiplies<T>()); }
    NDView &operator/=(const T val) { return elemenwise(val, std::divides<T>()); }

    NDView &operator/=(const NDView &other) { return elemenwise(other, std::divides<T>()); }


    template<size_t Size>
    NDView& operator=(const std::array<T, Size> &arr) {
        if(size() != static_cast<ssize_t>(arr.size()))
            throw std::runtime_error(LOCATION + "Array and NDView size mismatch");
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
    auto shape(int64_t i) const { return shape_[i]; }

    T *data() { return buffer_; }
    void print_all() const;

  private:
    T *buffer_{nullptr};
    std::array<int64_t, Ndim> strides_{};
    std::array<int64_t, Ndim> shape_{};
    uint64_t size_{};

    template <class BinaryOperation> NDView &elemenwise(T val, BinaryOperation op) {
        for (uint64_t i = 0; i != size_; ++i) {
            buffer_[i] = op(buffer_[i], val);
        }
        return *this;
    }
    template <class BinaryOperation> NDView &elemenwise(const NDView &other, BinaryOperation op) {
        for (uint64_t i = 0; i != size_; ++i) {
            buffer_[i] = op(buffer_[i], other.buffer_[i]);
        }
        return *this;
    }
};
template <typename T, int64_t Ndim> void NDView<T, Ndim>::print_all() const {
    for (auto row = 0; row < shape_[0]; ++row) {
        for (auto col = 0; col < shape_[1]; ++col) {
            std::cout << std::setw(3);
            std::cout << (*this)(row, col) << " ";
        }
        std::cout << "\n";
    }
}


template <typename T, int64_t Ndim>
std::ostream& operator <<(std::ostream& os, const NDView<T, Ndim>& arr){
    for (auto row = 0; row < arr.shape(0); ++row) {
        for (auto col = 0; col < arr.shape(1); ++col) {
            os << std::setw(3);
            os << arr(row, col) << " ";
        }
        os << "\n";
    }
    return os;
}


} // namespace aare