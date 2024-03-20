#pragma once
#include <algorithm>
#include <array>
#include <cassert>
#include <cstdint>
#include <numeric>
#include<aare/Frame.hpp>
#include <vector>


template <ssize_t Dim = 0, typename Strides> ssize_t element_offset(const Strides &) { return 0; }

template <ssize_t Dim = 0, typename Strides, typename... Ix>
ssize_t element_offset(const Strides &strides, ssize_t i, Ix... index) {
    return i * strides[Dim] + element_offset<Dim + 1>(strides, index...);
}

template <ssize_t Ndim> std::array<ssize_t, Ndim> c_strides(const std::array<ssize_t, Ndim> &shape) {
    std::array<ssize_t, Ndim> strides;
    std::fill(strides.begin(), strides.end(), 1);
    for (ssize_t i = Ndim - 1; i > 0; --i) {
        strides[i - 1] = strides[i] * shape[i];
    }
    return strides;
}

template <ssize_t Ndim> std::array<ssize_t, Ndim> make_array(const std::vector<ssize_t> &vec) {
    assert(vec.size() == Ndim);
    std::array<ssize_t, Ndim> arr;
    std::copy_n(vec.begin(), Ndim, arr.begin());
    return arr;
}

template <typename T, ssize_t Ndim> class DataSpan {
  public:
    DataSpan(){};

    DataSpan(T* buffer, std::array<ssize_t, Ndim> shape) {
        buffer_ = buffer;
        strides_ = c_strides<Ndim>(shape);
        shape_ = shape;
        size_ = std::accumulate(std::begin(shape), std::end(shape), 1, std::multiplies<ssize_t>());
    }

    DataSpan(T *buffer, const std::vector<ssize_t> &shape) {
        buffer_ = buffer;
        strides_ = c_strides<Ndim>(make_array<Ndim>(shape));
        shape_ = make_array<Ndim>(shape);
        size_ = std::accumulate(std::begin(shape), std::end(shape), 1, std::multiplies<ssize_t>());
        
    }

    DataSpan(Frame& frame) {
        assert(Ndim == 2);
        buffer_ = reinterpret_cast<T*>(frame._get_data());
        strides_ = c_strides<Ndim>({frame.rows(), frame.cols()});
        shape_ = {frame.rows(), frame.cols()};
        size_ = frame.rows() * frame.cols();
    }

    template <typename... Ix> typename std::enable_if<sizeof...(Ix) == Ndim, T &>::type operator()(Ix... index) {
        return buffer_[element_offset(strides_, index...)];
    }

    template <typename... Ix> typename std::enable_if<sizeof...(Ix) == Ndim, T &>::type operator()(Ix... index) const {
        return buffer_[element_offset(strides_, index...)];
    }

    ssize_t size() const { return size_; }

    DataSpan(const DataSpan &) = default;
    DataSpan(DataSpan &&) = default;

    T *begin() { return buffer_; }
    T *end() { return buffer_ + size_; }
    T &operator()(ssize_t i) { return buffer_[i]; }
    T &operator[](ssize_t i) { return buffer_[i]; }

    DataSpan &operator+=(const T val) { return elemenwise(val, std::plus<T>()); }
    DataSpan &operator-=(const T val) { return elemenwise(val, std::minus<T>()); }
    DataSpan &operator*=(const T val) { return elemenwise(val, std::multiplies<T>()); }
    DataSpan &operator/=(const T val) { return elemenwise(val, std::divides<T>()); }

    DataSpan &operator/=(const DataSpan &other) { return elemenwise(other, std::divides<T>()); }

    DataSpan &operator=(const T val) {
        for (auto it = begin(); it != end(); ++it)
            *it = val;
        return *this;
    }

    DataSpan &operator=(const DataSpan &other) {
        shape_ = other.shape_;
        strides_ = other.strides_;
        size_ = other.size_;
        buffer_ = other.buffer_;
        return *this;
    }
    auto &shape() { return shape_; }
    auto shape(ssize_t i) const { return shape_[i]; }

    T *data() { return buffer_; }

  private:
    T *buffer_{nullptr};
    std::array<ssize_t, Ndim> strides_{};
    std::array<ssize_t, Ndim> shape_{};
    ssize_t size_{};

    template <class BinaryOperation> DataSpan &elemenwise(T val, BinaryOperation op) {
        for (ssize_t i = 0; i != size_; ++i) {
            buffer_[i] = op(buffer_[i], val);
        }
        return *this;
    }
    template <class BinaryOperation> DataSpan &elemenwise(const DataSpan &other, BinaryOperation op) {
        for (ssize_t i = 0; i != size_; ++i) {
            buffer_[i] = op(buffer_[i], other.buffer_[i]);
        }
        return *this;
    }
};

template class DataSpan<uint16_t, 2>;

