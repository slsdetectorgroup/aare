#pragma once
/*
Container holding image data, or a time series of image data in contigious
memory.


TODO! Add expression templates for operators

*/
#include "aare/View.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <fmt/format.h>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <numeric>


template <typename T, ssize_t Ndim = 2> class Image {
  public:
    Image()
        : shape_(), strides_(c_strides<Ndim>(shape_)), size_(0),
          data_(nullptr){};

    explicit Image(std::array<ssize_t, Ndim> shape)
        : shape_(shape), strides_(c_strides<Ndim>(shape_)),
          size_(std::accumulate(shape_.begin(), shape_.end(), 1,
                                std::multiplies<ssize_t>())),
          data_(new T[size_]){};

    Image(std::array<ssize_t, Ndim> shape, T value) : Image(shape) {
        this->operator=(value);
    }

    /* When constructing from a View we need to copy the data since
    Image expect to own its data, and span is just a view*/
    Image(View<T, Ndim> span):Image(span.shape()){
        std::copy(span.begin(), span.end(), begin());
        // fmt::print("Image(View<T, Ndim> span)\n");
    }

    // Move constructor
    Image(Image &&other)
        : shape_(other.shape_), strides_(c_strides<Ndim>(shape_)),
          size_(other.size_), data_(nullptr) {
        data_ = other.data_;
        other.reset();
        // fmt::print("Image(Image &&other)\n");
    }

    // Copy constructor
    Image(const Image &other)
        : shape_(other.shape_), strides_(c_strides<Ndim>(shape_)),
          size_(other.size_), data_(new T[size_]) {
        std::copy(other.data_, other.data_ + size_, data_);
        // fmt::print("Image(const Image &other)\n");
    }

    ~Image() {
        delete[] data_;
    }

    auto begin() { return data_; }
    auto end() { return data_ + size_; }

    using value_type =  T;

    Image &operator=(Image &&other);      // Move assign
    Image &operator=(const Image &other); // Copy assign

    Image operator+(const Image &other);
    Image &operator+=(const Image &other);
    Image operator-(const Image &other);
    Image &operator-=(const Image &other);
    Image operator*(const Image &other);
    Image &operator*=(const Image &other);
    Image operator/(const Image &other);
    // Image& operator/=(const Image& other);
    template <typename V>
    Image &operator/=(const Image<V, Ndim> &other) {
        // check shape
        if (shape_ == other.shape()) {
            for (int i = 0; i < size_; ++i) {
                data_[i] /= other(i);
            }
            return *this;
        } else {
            throw(std::runtime_error("Shape of Image must match"));
        }
    }

    Image<bool, Ndim> operator>(const Image &other);

    bool operator==(const Image &other) const;
    bool operator!=(const Image &other) const;

    Image &operator=(const T &);
    Image &operator+=(const T &);
    Image operator+(const T &);
    Image &operator-=(const T &);
    Image operator-(const T &);
    Image &operator*=(const T &);
    Image operator*(const T &);
    Image &operator/=(const T &);
    Image operator/(const T &);

    Image &operator&=(const T &);

    void sqrt() {
        for (int i = 0; i < size_; ++i) {
            data_[i] = std::sqrt(data_[i]);
        }
    }

    Image &operator++(); // pre inc

    template <typename... Ix>
    typename std::enable_if<sizeof...(Ix) == Ndim, T &>::type
    operator()(Ix... index) {
        return data_[element_offset(strides_, index...)];
    }

    template <typename... Ix>
    typename std::enable_if<sizeof...(Ix) == Ndim, T &>::type
    operator()(Ix... index) const{
        return data_[element_offset(strides_, index...)];
    }


    template <typename... Ix>
    typename std::enable_if<sizeof...(Ix) == Ndim, T>::type value(Ix... index) {
        return data_[element_offset(strides_, index...)];
    }

    T &operator()(int i) { return data_[i]; }
    const T &operator()(int i) const { return data_[i]; }

    T *data() { return data_; }
    std::byte *buffer() { return reinterpret_cast<std::byte *>(data_); }
    ssize_t size() const { return size_; }
    size_t total_bytes() const {return size_*sizeof(T);}
    std::array<ssize_t, Ndim> shape() const noexcept { return shape_; }
    ssize_t shape(ssize_t i) const noexcept { return shape_[i]; }
    std::array<ssize_t, Ndim> strides() const noexcept { return strides_; }
    std::array<ssize_t, Ndim> byte_strides() const noexcept {
        auto byte_strides = strides_;
        for (auto &val : byte_strides)
            val *= sizeof(T);
        return byte_strides;
        // return strides_;
    }

    View<T, Ndim> span() const { return View<T, Ndim>{data_, shape_}; }

    void Print();
    void Print_all();
    void Print_some();

    void reset() {
        data_ = nullptr;
        size_ = 0;
        std::fill(shape_.begin(), shape_.end(), 0);
        std::fill(strides_.begin(), strides_.end(), 0);
    }

  private:
    std::array<ssize_t, Ndim> shape_;
    std::array<ssize_t, Ndim> strides_;
    ssize_t size_;
    T *data_;
};

// Move assign
template <typename T, ssize_t Ndim>
Image<T, Ndim> &Image<T, Ndim>::operator=(Image<T, Ndim> &&other) {
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

template <typename T, ssize_t Ndim>
Image<T, Ndim> Image<T, Ndim>::operator+(const Image &other) {
    Image result(*this);
    result += other;
    return result;
}
template <typename T, ssize_t Ndim>
Image<T, Ndim> &
Image<T, Ndim>::operator+=(const Image<T, Ndim> &other) {
    // check shape
    if (shape_ == other.shape_) {
        for (int i = 0; i < size_; ++i) {
            data_[i] += other.data_[i];
        }
        return *this;
    } else {
        throw(std::runtime_error("Shape of ImageDatas must match"));
    }
}

template <typename T, ssize_t Ndim>
Image<T, Ndim> Image<T, Ndim>::operator-(const Image &other) {
    Image result{*this};
    result -= other;
    return result;
}

template <typename T, ssize_t Ndim>
Image<T, Ndim> &
Image<T, Ndim>::operator-=(const Image<T, Ndim> &other) {
    // check shape
    if (shape_ == other.shape_) {
        for (int i = 0; i < size_; ++i) {
            data_[i] -= other.data_[i];
        }
        return *this;
    } else {
        throw(std::runtime_error("Shape of ImageDatas must match"));
    }
}
template <typename T, ssize_t Ndim>
Image<T, Ndim> Image<T, Ndim>::operator*(const Image &other) {
    Image result = *this;
    result *= other;
    return result;
}

template <typename T, ssize_t Ndim>
Image<T, Ndim> &
Image<T, Ndim>::operator*=(const Image<T, Ndim> &other) {
    // check shape
    if (shape_ == other.shape_) {
        for (int i = 0; i < size_; ++i) {
            data_[i] *= other.data_[i];
        }
        return *this;
    } else {
        throw(std::runtime_error("Shape of ImageDatas must match"));
    }
}

template <typename T, ssize_t Ndim>
Image<T, Ndim> Image<T, Ndim>::operator/(const Image &other) {
    Image result = *this;
    result /= other;
    return result;
}

template <typename T, ssize_t Ndim>
Image<T, Ndim> &Image<T, Ndim>::operator&=(const T &mask) {
    for (auto it = begin(); it != end(); ++it)
        *it &= mask;
    return *this;
}

// template <typename T, ssize_t Ndim>
// Image<T, Ndim>& Image<T, Ndim>::operator/=(const Image<T, Ndim>&
// other)
// {
//     //check shape
//     if (shape_ == other.shape_) {
//         for (int i = 0; i < size_; ++i) {
//             data_[i] /= other.data_[i];
//         }
//         return *this;
//     } else {
//         throw(std::runtime_error("Shape of ImageDatas must match"));
//     }
// }

template <typename T, ssize_t Ndim>
Image<bool, Ndim> Image<T, Ndim>::operator>(const Image &other) {
    if (shape_ == other.shape_) {
        Image<bool> result{shape_};
        for (int i = 0; i < size_; ++i) {
            result(i) = (data_[i] > other.data_[i]);
        }
        return result;
    } else {
        throw(std::runtime_error("Shape of ImageDatas must match"));
    }
}

template <typename T, ssize_t Ndim>
Image<T, Ndim> &
Image<T, Ndim>::operator=(const Image<T, Ndim> &other) {
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

template <typename T, ssize_t Ndim>
bool Image<T, Ndim>::operator==(const Image<T, Ndim> &other) const {
    if (shape_ != other.shape_)
        return false;

    for (int i = 0; i != size_; ++i)
        if (data_[i] != other.data_[i])
            return false;

    return true;
}

template <typename T, ssize_t Ndim>
bool Image<T, Ndim>::operator!=(const Image<T, Ndim> &other) const {
    return !((*this) == other);
}
template <typename T, ssize_t Ndim>
Image<T, Ndim> &Image<T, Ndim>::operator++() {
    for (int i = 0; i < size_; ++i)
        data_[i] += 1;
    return *this;
}
template <typename T, ssize_t Ndim>
Image<T, Ndim> &Image<T, Ndim>::operator=(const T &value) {
    std::fill_n(data_, size_, value);
    return *this;
}

template <typename T, ssize_t Ndim>
Image<T, Ndim> &Image<T, Ndim>::operator+=(const T &value) {
    for (int i = 0; i < size_; ++i)
        data_[i] += value;
    return *this;
}

template <typename T, ssize_t Ndim>
Image<T, Ndim> Image<T, Ndim>::operator+(const T &value) {
    Image result = *this;
    result += value;
    return result;
}
template <typename T, ssize_t Ndim>
Image<T, Ndim> &Image<T, Ndim>::operator-=(const T &value) {
    for (int i = 0; i < size_; ++i)
        data_[i] -= value;
    return *this;
}
template <typename T, ssize_t Ndim>
Image<T, Ndim> Image<T, Ndim>::operator-(const T &value) {
    Image result = *this;
    result -= value;
    return result;
}

template <typename T, ssize_t Ndim>
Image<T, Ndim> &Image<T, Ndim>::operator/=(const T &value) {
    for (int i = 0; i < size_; ++i)
        data_[i] /= value;
    return *this;
}
template <typename T, ssize_t Ndim>
Image<T, Ndim> Image<T, Ndim>::operator/(const T &value) {
    Image result = *this;
    result /= value;
    return result;
}
template <typename T, ssize_t Ndim>
Image<T, Ndim> &Image<T, Ndim>::operator*=(const T &value) {
    for (int i = 0; i < size_; ++i)
        data_[i] *= value;
    return *this;
}
template <typename T, ssize_t Ndim>
Image<T, Ndim> Image<T, Ndim>::operator*(const T &value) {
    Image result = *this;
    result *= value;
    return result;
}
template <typename T, ssize_t Ndim> void Image<T, Ndim>::Print() {
    if (shape_[0] < 20 && shape_[1] < 20)
        Print_all();
    else
        Print_some();
}
template <typename T, ssize_t Ndim> void Image<T, Ndim>::Print_all() {
    for (auto row = 0; row < shape_[0]; ++row) {
        for (auto col = 0; col < shape_[1]; ++col) {
            std::cout << std::setw(3);
            std::cout << (*this)(row, col) << " ";
        }
        std::cout << "\n";
    }
}
template <typename T, ssize_t Ndim> void Image<T, Ndim>::Print_some() {
    for (auto row = 0; row < 5; ++row) {
        for (auto col = 0; col < 5; ++col) {
            std::cout << std::setw(7);
            std::cout << (*this)(row, col) << " ";
        }
        std::cout << "\n";
    }
}

template <typename T, ssize_t Ndim>
void save(Image<T, Ndim> &img, std::string pathname) {
    std::ofstream f;
    f.open(pathname, std::ios::binary);
    f.write(img.buffer(), img.size() * sizeof(T));
    f.close();
}

template <typename T, ssize_t Ndim>
Image<T, Ndim> load(const std::string &pathname,
                        std::array<ssize_t, Ndim> shape) {
    Image<T, Ndim> img{shape};
    std::ifstream f;
    f.open(pathname, std::ios::binary);
    f.read(img.buffer(), img.size() * sizeof(T));
    f.close();
    return img;
}


