// SPDX-License-Identifier: MPL-2.0
//
// Container holding image data, or a time series of image data in contigious
// memory. Used for all data processing in Aare.
//

#pragma once
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

template <typename T, ssize_t Ndim = 2>
class NDArray : public ArrayExpr<NDArray<T, Ndim>, Ndim> {
    std::array<ssize_t, Ndim> shape_;
    std::array<ssize_t, Ndim> strides_;
    size_t size_{}; // TODO! do we need to store size when we have shape?
    T *data_;

  public:
    ///////////////////////////////////////////////////////////////////////////////
    // Constructors
    //
    ///////////////////////////////////////////////////////////////////////////////

    /**
     * @brief Default constructor. Constructs an empty NDArray.
     *
     */
    NDArray() : shape_(), strides_(c_strides<Ndim>(shape_)), data_(nullptr) {};

    /**
     * @brief Construct a new NDArray object with a given shape.
     * @note The data is uninitialized.
     *
     * @param shape shape of the new NDArray
     */
    explicit NDArray(std::array<ssize_t, Ndim> shape)
        : shape_(shape), strides_(c_strides<Ndim>(shape_)),
          size_(num_elements(shape_)), data_(new T[size_]) {}

    /**
     * @brief Construct a new NDArray object with a shape and value.
     *
     * @param shape shape of the new array
     * @param value value to initialize the array with
     */
    NDArray(std::array<ssize_t, Ndim> shape, T value) : NDArray(shape) {
        this->operator=(value);
    }

    // Allow NDArray of different type and dimension to be friend classes
    // This is needed for the move constructor from NDArray<T,Ndim+1>
    template <typename U, ssize_t Dim> friend class NDArray;

    /**
     * @brief Construct a new NDArray object from a NDView.
     * @note The data is copied from the view to the NDArray.
     *
     * @param v view of data to initialize the NDArray with
     */
    explicit NDArray(const NDView<T, Ndim> v) : NDArray(v.shape()) {
        std::copy(v.begin(), v.end(), begin());
    }

    /**
     * @brief Construct a new NDArray object from an std::array.
     */
    template <size_t Size>
    NDArray(const std::array<T, Size> &arr) : NDArray<T, 1>({Size}) {
        std::copy(arr.begin(), arr.end(), begin());
    }

    /**
     * @brief Move construct a new NDArray object. Cheap since it just
     * reassigns the pointer and copy size/strides.
     *
     * @param other
     */
    NDArray(NDArray &&other) noexcept
        : shape_(other.shape_), strides_(c_strides<Ndim>(shape_)),
          size_(other.size_), data_(other.data_) {
        other.reset(); // Needed to avoid double free
    }

    /**
     * @brief Move construct a new NDArray object from an array with Ndim + 1.
     * Can be used to drop a dimension cheaply.
     * @param other
     */
    template <ssize_t M, typename = std::enable_if_t<(M == Ndim + 1)>>
    NDArray(NDArray<T, M> &&other)
        : shape_(drop_first_dim(other.shape())),
          strides_(c_strides<Ndim>(shape_)), size_(num_elements(shape_)),
          data_(other.data()) {

        // For now only allow move if the size matches, to avoid unreachable
        // data if the use case arises we can remove this check
        if (size() != other.size()) {
            data_ = nullptr; // avoid double free, other will clean up the
                             // memory in it's destructor
            throw std::runtime_error(
                LOCATION +
                "Size mismatch in move constructor of NDArray<T, Ndim-1>");
        }
        other.reset();
    }

    /**
     * @brief Copy construct a new NDArray object from another NDArray.
     *
     * @param other
     */
    NDArray(const NDArray &other)
        : shape_(other.shape_), strides_(c_strides<Ndim>(shape_)),
          size_(other.size_), data_(new T[size_]) {
        std::copy(other.data_, other.data_ + size_, data_);
    }

    /**
     * @brief Conversion from a ArrayExpr to an actual NDArray. Used when
     * the expression is evaluated and data needed.
     *
     * @tparam E
     * @param expr
     */
    template <typename E>
    NDArray(ArrayExpr<E, Ndim> &&expr) : NDArray(expr.shape()) {
        for (size_t i = 0; i < size_; ++i) {
            data_[i] = expr[i];
        }
    }

    /**
     * @brief Destroy the NDArray object. Frees the allocated memory.
     *
     */
    ~NDArray() { delete[] data_; }

    ///////////////////////////////////////////////////////////////////////////////
    // Iterators and indexing
    //
    ///////////////////////////////////////////////////////////////////////////////

    auto *begin() { return data_; }
    const auto *begin() const { return data_; }

    auto *end() { return data_ + size_; }
    const auto *end() const { return data_ + size_; }

    /*
     * @brief Access element at given multi-dimensional index.
     * i.e. arr(i,j,k,...)
     *
     * @note The fast index is the last index. Please take care when iterating
     * through the array.
     */
    template <typename... Ix>
    std::enable_if_t<sizeof...(Ix) == Ndim, T &> operator()(Ix... index) {
        return data_[element_offset(strides_, index...)];
    }

    /*
     * @brief Access element at given multi-dimensional index (const version).
     * i.e. arr(i,j,k,...)
     *
     * @note The fast index is the last index. Please take care when iterating
     * through the array.
     */
    template <typename... Ix>
    std::enable_if_t<sizeof...(Ix) == Ndim, const T &>
    operator()(Ix... index) const {
        return data_[element_offset(strides_, index...)];
    }

    /*
     @brief Index the array as it would be a 1D array. To get a certain
     pixel in a multidimensional array use the (i,j,k,...) operator instead.
     */
    T &operator()(ssize_t i) { return data_[i]; }

    /*
     @brief Index the array as it would be a 1D array. To get a certain
     pixel in a multidimensional array use the (i,j,k,...) operator instead.
     */
    const T &operator()(ssize_t i) const { return data_[i]; }

    /*
     @brief Index the array as it would be a 1D array. To get a certain
     pixel in a multidimensional array use the (i,j,k,...) operator instead.
     */
    T &operator[](ssize_t i) { return data_[i]; }

    /*
     @brief Index the array as it would be a 1D array. To get a certain
     pixel in a multidimensional array use the (i,j,k,...) operator instead.
     */
    const T &operator[](ssize_t i) const { return data_[i]; }

    /* @brief Return a raw pointer to the data */
    T *data() { return data_; }

    /* @brief Return a const raw pointer to the data */
    const T *data() const { return data_; }

    /* @brief Return a byte pointer to the data. Useful for memcpy like
     * operations */
    std::byte *buffer() { return reinterpret_cast<std::byte *>(data_); }

    /**
     * @brief Return the total number of elements in the array as a signed
     * integer
     * @note Is there a need for unsigned size?
     */
    ssize_t size() const { return static_cast<ssize_t>(size_); }

    /** @brief Return the total number of bytes in the array */
    size_t total_bytes() const { return size_ * sizeof(T); }

    /** @brief Return the shape of the array */
    Shape<Ndim> shape() const noexcept { return shape_; }

    /** @brief Return the size of dimension i */
    ssize_t shape(ssize_t i) const noexcept { return shape_[i]; }

    /** @brief Return the strides of the array */
    std::array<ssize_t, Ndim> strides() const noexcept { return strides_; }

    /**
     * @brief Return the bitdepth of the array. Useful for checking that
     * detector data can fit in the array type.
     */
    size_t bitdepth() const noexcept { return sizeof(T) * 8; }

    /**
     * @brief Return the number of bytes to step in each dimension when
     * traversing the array.
     */
    std::array<ssize_t, Ndim> byte_strides() const noexcept {
        auto byte_strides = strides_;
        for (auto &val : byte_strides)
            val *= sizeof(T);
        return byte_strides;
    }

    using value_type = T;

    ///////////////////////////////////////////////////////////////////////////////
    // Assignments
    //
    ///////////////////////////////////////////////////////////////////////////////

    /**
     * @brief Copy to the NDArray from an std::array. If the size of the array
     * is different we reallocate the data.
     *
     */
    template <size_t Size>
    NDArray<T, 1> &operator=(const std::array<T, Size> &other) {
        if (Size != size_) {
            delete[] data_;
            size_ = Size;
            data_ = new T[size_];
        }
        for (size_t i = 0; i < Size; ++i) {
            data_[i] = other[i];
        }
        return *this;
    }

    /**
     * @brief Move assignment operator.
     */
    NDArray &operator=(NDArray &&other) noexcept {
        // TODO! Should we use swap?
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

    /**
     * @brief Copy assignment operator.
     */
    NDArray &operator=(const NDArray &other) {
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

    ///////////////////////////////////////////////////////////////////////////////
    // Math operators
    //
    ///////////////////////////////////////////////////////////////////////////////

    /**
     * @brief Add elementwise from another NDArray.
     */
    NDArray &operator+=(const NDArray &other) {
        if (shape_ != other.shape_)
            throw(std::runtime_error(
                "Shape of NDArray must match for operator +="));

        for (size_t i = 0; i < size_; ++i) {
            data_[i] += other.data_[i];
        }
        return *this;
    }

    /**
     * @brief Subtract elementwise with another NDArray.
     */
    NDArray &operator-=(const NDArray &other) {
        if (shape_ != other.shape_)
            throw(std::runtime_error(
                "Shape of NDArray must match for operator -="));

        for (size_t i = 0; i < size_; ++i) {
            data_[i] -= other.data_[i];
        }
        return *this;
    }

    /**
     * @brief Multiply elementwise with another NDArray.
     */
    NDArray &operator*=(const NDArray &other) {
        if (shape_ != other.shape_)
            throw(std::runtime_error(
                "Shape of NDArray must match for operator *="));

        for (size_t i = 0; i < size_; ++i) {
            data_[i] *= other.data_[i];
        }
        return *this;
    }

    /**
     * @brief Divide elementwise by another NDArray. Templated to allow division
     * with different types.
     *
     * TODO! Why is this templated when the others are not?
     */
    template <typename V> NDArray &operator/=(const NDArray<V, Ndim> &other) {
        // check shape
        if (shape_ == other.shape()) {
            for (size_t i = 0; i < size_; ++i) {
                data_[i] /= other(i);
            }
            return *this;
        }
        throw(std::runtime_error("Shape of NDArray must match"));
    }

    /**
     * @brief Assign a scalar value to all elements in the NDArray.
     */
    NDArray &operator=(const T &value) {
        std::fill_n(data_, size_, value);
        return *this;
    }

    /**
     * @brief Add a scalar value to all elements in the NDArray.
     */
    NDArray &operator+=(const T &value) {
        for (size_t i = 0; i < size_; ++i)
            data_[i] += value;
        return *this;
    }

    /**
     * @brief Subtract a scalar value to all elements in the NDArray.
     */
    NDArray &operator-=(const T &value) {
        for (size_t i = 0; i < size_; ++i)
            data_[i] -= value;
        return *this;
    }

    /**
     * @brief Multiply all elements in the NDArray with a scalar value
     */
    NDArray &operator*=(const T &value) {
        for (size_t i = 0; i < size_; ++i)
            data_[i] *= value;
        return *this;
    }

    /**
     * @brief Divide all elements in the NDArray with a scalar value
     */
    NDArray &operator/=(const T &value) {
        for (size_t i = 0; i < size_; ++i)
            data_[i] /= value;
        return *this;
    }

    /**
     * @brief Bitwise AND all elements in the NDArray with a scalar mask.
     * Used for example to mask out gain bits for Jungfrau detectors.
     */
    NDArray &operator&=(const T &mask) {
        for (auto it = begin(); it != end(); ++it)
            *it &= mask;
        return *this;
    }

    /**
     * @brief Operator +  with a scalar value. Returns a new NDArray.
     *
     * TODO! Expression template version of this?
     */
    NDArray operator+(const T &value) {
        NDArray result = *this;
        result += value;
        return result;
    }

    /**
     * @brief Operator -  with a scalar value. Returns a new NDArray.
     *
     * TODO! Expression template version of this?
     */
    NDArray operator-(const T &value) {
        NDArray result = *this;
        result -= value;
        return result;
    }

    /**
     * @brief Operator *  with a scalar value. Returns a new NDArray.
     *
     * TODO! Expression template version of this?
     */
    NDArray operator*(const T &value) {
        NDArray result = *this;
        result *= value;
        return result;
    }

    /**
     * @brief Operator /  with a scalar value. Returns a new NDArray.
     *
     * TODO! Expression template version of this?
     */
    NDArray operator/(const T &value) {
        NDArray result = *this;
        result /= value;
        return result;
    }

    /**
     * @brief Compare two NDArrays elementwise for equality.
     */
    bool operator==(const NDArray &other) const {
        if (shape_ != other.shape_)
            return false;

        for (size_t i = 0; i != size_; ++i)
            if (data_[i] != other.data_[i])
                return false;

        return true;
    }

    /**
     * @brief Compare two NDArrays elementwise for non-equality.
     */
    bool operator!=(const NDArray &other) const { return !((*this) == other); }

    /**
     * @brief Compute the square root of all elements in the NDArray.
     */
    void sqrt() {
        for (size_t i = 0; i < size_; ++i) {
            data_[i] = std::sqrt(data_[i]);
        }
    }

    /*
     * @brief Prefix increment operator. Increments all elements by 1.
     */
    NDArray &operator++() {
        for (size_t i = 0; i < size_; ++i)
            data_[i] += T{1};
        return *this;
    }

    /**
     * @brief Create a view of the NDArray.
     *
     * @return NDView<T, Ndim>
     */
    NDView<T, Ndim> view() const { return NDView<T, Ndim>{data_, shape_}; }

  private:
    /**
     * @brief Reset the NDArray to an empty state. Dropping the ownership of
     * the data. Used internally for move operations to avoid double free or
     * dangling pointers.
     */
    void reset() {
        data_ = nullptr;
        size_ = 0;
        std::fill(shape_.begin(), shape_.end(), 0);
        std::fill(strides_.begin(), strides_.end(), 0);
    }
};

///////////////////////////////////////////////////////////////////////////////
// Free functions closely related to NDArray
//
///////////////////////////////////////////////////////////////////////////////

template <typename T, ssize_t Ndim>
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

template <typename T, ssize_t Ndim>
[[deprecated("Saving of raw arrays without metadata is deprecated")]] void
save(NDArray<T, Ndim> &img, std::string &pathname) {
    std::ofstream f;
    f.open(pathname, std::ios::binary);
    f.write(img.buffer(), img.size() * sizeof(T));
    f.close();
}

template <typename T, ssize_t Ndim>
[[deprecated(
    "Loading of raw arrays without metadata is deprecated")]] NDArray<T, Ndim>
load(const std::string &pathname, std::array<ssize_t, Ndim> shape) {
    NDArray<T, Ndim> img{shape};
    std::ifstream f;
    f.open(pathname, std::ios::binary);
    f.read(img.buffer(), img.size() * sizeof(T));
    f.close();
    return img;
}

/**
 * @brief Free function to safely divide two NDArrays elementwise, handling
 * division by zero. Uses static_cast to convert types as needed.
 *
 * @tparam RT Result type
 * @tparam NT Numerator type
 * @tparam DT Denominator type
 * @tparam Ndim Number of dimensions
 * @param numerator The numerator NDArray
 * @param denominator The denominator NDArray
 * @return NDArray<RT, Ndim> Resulting NDArray after safe division
 * @throws std::runtime_error if the shapes of the numerator and denominator do
 * not match
 */
template <typename RT, typename NT, typename DT, ssize_t Ndim>
NDArray<RT, Ndim> safe_divide(const NDArray<NT, Ndim> &numerator,
                              const NDArray<DT, Ndim> &denominator) {
    if (numerator.shape() != denominator.shape()) {
        throw std::runtime_error(
            "Shapes of numerator and denominator must match");
    }
    NDArray<RT, Ndim> result(numerator.shape());
    for (ssize_t i = 0; i < numerator.size(); ++i) {
        if (denominator[i] != 0) {
            result[i] =
                static_cast<RT>(numerator[i]) / static_cast<RT>(denominator[i]);
        } else {
            result[i] = RT{0}; // or handle division by zero as needed
        }
    }
    return result;
}

} // namespace aare
