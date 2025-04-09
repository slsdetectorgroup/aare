#pragma once
/*
Container holding image data, or a time series of image data in contigious
memory.


TODO! Add expression templates for operators

*/
#include "aare/ArrayExpr.hpp"
#include "aare/NDView.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <fmt/format.h>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <numeric>

namespace aare {


template <typename T, int64_t Ndim = 2>
class NDArray : public ArrayExpr<NDArray<T, Ndim>, Ndim> {
    std::array<int64_t, Ndim> shape_;
    std::array<int64_t, Ndim> strides_;
    size_t size_{};
    T *data_;

  public:
    /**
     * @brief Default constructor. Will construct an empty NDArray.
     *
     */
    NDArray() : shape_(), strides_(c_strides<Ndim>(shape_)), data_(nullptr) {};

    /**
     * @brief Construct a new NDArray object with a given shape.
     * @note The data is uninitialized.
     *
     * @param shape shape of the new NDArray
     */
    explicit NDArray(std::array<int64_t, Ndim> shape)
        : shape_(shape), strides_(c_strides<Ndim>(shape_)),
          size_(std::accumulate(shape_.begin(), shape_.end(), 1,
                                std::multiplies<>())),
          data_(new T[size_]) {}


    /**
     * @brief Construct a new NDArray object with a shape and value.
     *
     * @param shape shape of the new array
     * @param value value to initialize the array with
     */
    NDArray(std::array<int64_t, Ndim> shape, T value) : NDArray(shape) {
        this->operator=(value);
    }

    /**
     * @brief Construct a new NDArray object from a NDView.
     * @note The data is copied from the view to the NDArray.
     *
     * @param v view of data to initialize the NDArray with
     */
    explicit NDArray(const NDView<T, Ndim> v) : NDArray(v.shape()) {
        std::copy(v.begin(), v.end(), begin());
    }

    template<size_t Size>
    NDArray(const std::array<T, Size>& arr) : NDArray<T,1>({Size}) {
        std::copy(arr.begin(), arr.end(), begin());
    }

    // Move constructor
    NDArray(NDArray &&other) noexcept
        : shape_(other.shape_), strides_(c_strides<Ndim>(shape_)),
          size_(other.size_), data_(other.data_) {
        other.reset(); // TODO! is this necessary?

    }

    // Copy constructor
    NDArray(const NDArray &other)
        : shape_(other.shape_), strides_(c_strides<Ndim>(shape_)),
          size_(other.size_), data_(new T[size_]) {
        std::copy(other.data_, other.data_ + size_, data_);
    }

    // Conversion operator from array expression to array
    template <typename E>
    NDArray(ArrayExpr<E, Ndim> &&expr) : NDArray(expr.shape()) {
        for (size_t i = 0; i < size_; ++i) {
            data_[i] = expr[i];
        }
    }

    ~NDArray() { delete[] data_; }

    auto begin() { return data_; }
    auto end() { return data_ + size_; }

    auto begin() const { return data_; }
    auto end() const { return data_ + size_; }

    using value_type = T;

    NDArray &operator=(NDArray &&other) noexcept; // Move assign
    NDArray &operator=(const NDArray &other);     // Copy assign
    NDArray &operator+=(const NDArray &other);
    NDArray &operator-=(const NDArray &other);
    NDArray &operator*=(const NDArray &other);

    //Write directly to the data array, or create a new one
    template<size_t Size>
    NDArray<T,1>& operator=(const std::array<T,Size> &other){
        if(Size != size_){
            delete[] data_;
            size_ = Size;
            data_ = new T[size_];
        }
        for (size_t i = 0; i < Size; ++i) {
            data_[i] = other[i];
        }
        return *this;
    }

    // NDArray& operator/=(const NDArray& other);

    template <typename V> NDArray &operator/=(const NDArray<V, Ndim> &other) {
        // check shape
        if (shape_ == other.shape()) {
            for (uint32_t i = 0; i < size_; ++i) {
                data_[i] /= other(i);
            }
            return *this;
        }
        throw(std::runtime_error("Shape of NDArray must match"));
    }

    NDArray<bool, Ndim> operator>(const NDArray &other);

    bool operator==(const NDArray &other) const;
    bool operator!=(const NDArray &other) const;

    NDArray &operator=(const T & /*value*/);
    NDArray &operator+=(const T & /*value*/);
    NDArray operator+(const T & /*value*/);
    NDArray &operator-=(const T & /*value*/);
    NDArray operator-(const T & /*value*/);
    NDArray &operator*=(const T & /*value*/);
    NDArray operator*(const T & /*value*/);
    NDArray &operator/=(const T & /*value*/);
    NDArray operator/(const T & /*value*/);

    NDArray &operator&=(const T & /*mask*/);

  


    

    void sqrt() {
        for (int i = 0; i < size_; ++i) {
            data_[i] = std::sqrt(data_[i]);
        }
    }

    NDArray &operator++(); // pre inc

    template <typename... Ix>
    std::enable_if_t<sizeof...(Ix) == Ndim, T &> operator()(Ix... index) {
        return data_[element_offset(strides_, index...)];
    }

    template <typename... Ix>
    std::enable_if_t<sizeof...(Ix) == Ndim, T &> operator()(Ix... index) const {
        return data_[element_offset(strides_, index...)];
    }

    template <typename... Ix>
    std::enable_if_t<sizeof...(Ix) == Ndim, T> value(Ix... index) {
        return data_[element_offset(strides_, index...)];
    }

    // TODO! is int the right type for index?
    T &operator()(int64_t i) { return data_[i]; }
    const T &operator()(int64_t i) const { return data_[i]; }

    T &operator[](int64_t i) { return data_[i]; }
    const T &operator[](int64_t i) const { return data_[i]; }

    T *data() { return data_; }
    std::byte *buffer() { return reinterpret_cast<std::byte *>(data_); }
    ssize_t size() const { return static_cast<ssize_t>(size_); }
    size_t total_bytes() const { return size_ * sizeof(T); }
    std::array<int64_t, Ndim> shape() const noexcept { return shape_; }
    int64_t shape(int64_t i) const noexcept { return shape_[i]; }
    std::array<int64_t, Ndim> strides() const noexcept { return strides_; }
    size_t bitdepth() const noexcept { return sizeof(T) * 8; }

    std::array<int64_t, Ndim> byte_strides() const noexcept {
        auto byte_strides = strides_;
        for (auto &val : byte_strides)
            val *= sizeof(T);
        return byte_strides;
    }

    /**
     * @brief Create a view of the NDArray.
     *
     * @return NDView<T, Ndim>
     */
    NDView<T, Ndim> view() const { return NDView<T, Ndim>{data_, shape_}; }

    void Print();
    void Print_all();
    void Print_some();

    void reset() {
        data_ = nullptr;
        size_ = 0;
        std::fill(shape_.begin(), shape_.end(), 0);
        std::fill(strides_.begin(), strides_.end(), 0);
    }
};

// Move assign
template <typename T, int64_t Ndim>
NDArray<T, Ndim> &
NDArray<T, Ndim>::operator=(NDArray<T, Ndim> &&other) noexcept {
    if (this != &other) {
        delete[] data_;
        data_ = other.data_;
        shape_ = other.shape_;
        size_ = other.size_;
        strides_ = other.strides_;
        other.reset();
    }
    return *this;
}

template <typename T, int64_t Ndim>
NDArray<T, Ndim> &NDArray<T, Ndim>::operator+=(const NDArray<T, Ndim> &other) {
    // check shape
    if (shape_ == other.shape_) {
        for (size_t i = 0; i < size_; ++i) {
            data_[i] += other.data_[i];
        }
        return *this;
    }
    throw(std::runtime_error("Shape of ImageDatas must match"));
}

template <typename T, int64_t Ndim>
NDArray<T, Ndim> &NDArray<T, Ndim>::operator-=(const NDArray<T, Ndim> &other) {
    // check shape
    if (shape_ == other.shape_) {
        for (uint32_t i = 0; i < size_; ++i) {
            data_[i] -= other.data_[i];
        }
        return *this;
    }
    throw(std::runtime_error("Shape of ImageDatas must match"));
}

template <typename T, int64_t Ndim>
NDArray<T, Ndim> &NDArray<T, Ndim>::operator*=(const NDArray<T, Ndim> &other) {
    // check shape
    if (shape_ == other.shape_) {
        for (uint32_t i = 0; i < size_; ++i) {
            data_[i] *= other.data_[i];
        }
        return *this;
    }
    throw(std::runtime_error("Shape of ImageDatas must match"));
}

template <typename T, int64_t Ndim>
NDArray<T, Ndim> &NDArray<T, Ndim>::operator&=(const T &mask) {
    for (auto it = begin(); it != end(); ++it)
        *it &= mask;
    return *this;
}

template <typename T, int64_t Ndim>
NDArray<bool, Ndim> NDArray<T, Ndim>::operator>(const NDArray &other) {
    if (shape_ == other.shape_) {
        NDArray<bool, Ndim> result{shape_};
        for (int i = 0; i < size_; ++i) {
            result(i) = (data_[i] > other.data_[i]);
        }
        return result;
    }
    throw(std::runtime_error("Shape of ImageDatas must match"));
}

template <typename T, int64_t Ndim>
NDArray<T, Ndim> &NDArray<T, Ndim>::operator=(const NDArray<T, Ndim> &other) {
    if (this != &other) {
        delete[] data_;
        shape_ = other.shape_;
        strides_ = other.strides_;
        size_ = other.size_;
        data_ = new T[size_];
        std::copy(other.data_, other.data_ + size_, data_);
    }
    return *this;
}

template <typename T, int64_t Ndim>
bool NDArray<T, Ndim>::operator==(const NDArray<T, Ndim> &other) const {
    if (shape_ != other.shape_)
        return false;

    for (uint32_t i = 0; i != size_; ++i)
        if (data_[i] != other.data_[i])
            return false;

    return true;
}

template <typename T, int64_t Ndim>
bool NDArray<T, Ndim>::operator!=(const NDArray<T, Ndim> &other) const {
    return !((*this) == other);
}
template <typename T, int64_t Ndim>
NDArray<T, Ndim> &NDArray<T, Ndim>::operator++() {
    for (uint32_t i = 0; i < size_; ++i)
        data_[i] += 1;
    return *this;
}
template <typename T, int64_t Ndim>
NDArray<T, Ndim> &NDArray<T, Ndim>::operator=(const T &value) {
    std::fill_n(data_, size_, value);
    return *this;
}

template <typename T, int64_t Ndim>
NDArray<T, Ndim> &NDArray<T, Ndim>::operator+=(const T &value) {
    for (uint32_t i = 0; i < size_; ++i)
        data_[i] += value;
    return *this;
}




template <typename T, int64_t Ndim>
NDArray<T, Ndim> NDArray<T, Ndim>::operator+(const T &value) {
    NDArray result = *this;
    result += value;
    return result;
}
template <typename T, int64_t Ndim>
NDArray<T, Ndim> &NDArray<T, Ndim>::operator-=(const T &value) {
    for (uint32_t i = 0; i < size_; ++i)
        data_[i] -= value;
    return *this;
}
template <typename T, int64_t Ndim>
NDArray<T, Ndim> NDArray<T, Ndim>::operator-(const T &value) {
    NDArray result = *this;
    result -= value;
    return result;
}

template <typename T, int64_t Ndim>
NDArray<T, Ndim> &NDArray<T, Ndim>::operator/=(const T &value) {
    for (uint32_t i = 0; i < size_; ++i)
        data_[i] /= value;
    return *this;
}
template <typename T, int64_t Ndim>
NDArray<T, Ndim> NDArray<T, Ndim>::operator/(const T &value) {
    NDArray result = *this;
    result /= value;
    return result;
}
template <typename T, int64_t Ndim>
NDArray<T, Ndim> &NDArray<T, Ndim>::operator*=(const T &value) {
    for (uint32_t i = 0; i < size_; ++i)
        data_[i] *= value;
    return *this;
}
template <typename T, int64_t Ndim>
NDArray<T, Ndim> NDArray<T, Ndim>::operator*(const T &value) {
    NDArray result = *this;
    result *= value;
    return result;
}
// template <typename T, int64_t Ndim> void NDArray<T, Ndim>::Print() {
//     if (shape_[0] < 20 && shape_[1] < 20)
//         Print_all();
//     else
//         Print_some();
// }

template <typename T, int64_t Ndim>
std::ostream &operator<<(std::ostream &os, const NDArray<T, Ndim> &arr) {
    for (auto row = 0; row < arr.shape(0); ++row) {
        for (auto col = 0; col < arr.shape(1); ++col) {
            os << std::setw(3);
            os << arr(row, col) << " ";
        }
        os << "\n";
    }
    return os;
}

template <typename T, int64_t Ndim> void NDArray<T, Ndim>::Print_all() {
    for (auto row = 0; row < shape_[0]; ++row) {
        for (auto col = 0; col < shape_[1]; ++col) {
            std::cout << std::setw(3);
            std::cout << (*this)(row, col) << " ";
        }
        std::cout << "\n";
    }
}
template <typename T, int64_t Ndim> void NDArray<T, Ndim>::Print_some() {
    for (auto row = 0; row < 5; ++row) {
        for (auto col = 0; col < 5; ++col) {
            std::cout << std::setw(7);
            std::cout << (*this)(row, col) << " ";
        }
        std::cout << "\n";
    }
}

template <typename T, int64_t Ndim>
void save(NDArray<T, Ndim> &img, std::string &pathname) {
    std::ofstream f;
    f.open(pathname, std::ios::binary);
    f.write(img.buffer(), img.size() * sizeof(T));
    f.close();
}

template <typename T, int64_t Ndim>
NDArray<T, Ndim> load(const std::string &pathname,
                      std::array<int64_t, Ndim> shape) {
    NDArray<T, Ndim> img{shape};
    std::ifstream f;
    f.open(pathname, std::ios::binary);
    f.read(img.buffer(), img.size() * sizeof(T));
    f.close();
    return img;
}



} // namespace aare