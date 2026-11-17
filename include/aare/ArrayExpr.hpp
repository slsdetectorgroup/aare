// SPDX-License-Identifier: MPL-2.0
#pragma once
#include "aare/defs.hpp"
#include <array>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <stdexcept>
#include <type_traits>

namespace aare {

template <typename E, ssize_t Ndim> class ArrayExpr {
  public:
    static constexpr bool is_leaf = false;

    auto operator[](size_t i) const { return static_cast<E const &>(*this)[i]; }
    auto operator()(size_t i) const { return static_cast<E const &>(*this)[i]; }
    auto size() const { return static_cast<E const &>(*this).size(); }
    std::array<ssize_t, Ndim> shape() const {
        return static_cast<E const &>(*this).shape();
    }
};

template <ssize_t Dim = 0, typename Strides>
ssize_t element_offset(const Strides & /*unused*/) {
    return 0;
}

template <ssize_t Dim = 0, typename Strides, typename... Ix>
ssize_t element_offset(const Strides &strides, ssize_t i, Ix... index) {
    return i * strides[Dim] + element_offset<Dim + 1>(strides, index...);
}

/**
 * @brief What an expression stores for one of its operands. An NDArray is
 * replaced by a view, everything else is copied. The expression then holds its
 * data pointers by value, so the compiler can keep them in registers even if
 * the evaluation writes to a type that may alias them (e.g. uint8_t).
 */
template <typename E, ssize_t Ndim>
auto operand(const ArrayExpr<E, Ndim> &expr) {
    if constexpr (E::is_leaf)
        return static_cast<const E &>(expr).view();
    else
        return static_cast<const E &>(expr);
}

/**
 * @brief Element access, elementwise math and comparison shared by NDArray
 * and NDView.
 *
 * CRTP base that sits between the container and ArrayExpr. Derived has to
 * provide data() (const and non-const), size(), shape() and strides(). The
 * containers pull the index operators in with using declarations, since their
 * own operator() overloads would otherwise hide them.
 *
 * @note strides() has to return a reference. With a copy clang reloads the
 * strides after every store through the returned element, which blocks
 * vectorization of loops that write with arr(i,j,...).
 *
 * @tparam Derived the container, NDArray<T, Ndim> or NDView<T, Ndim>
 * @tparam T element type
 * @tparam Ndim number of dimensions
 */
template <typename Derived, typename T, ssize_t Ndim>
class NDOps : public ArrayExpr<Derived, Ndim> {
  public:
    ///////////////////////////////////////////////////////////////////////////
    // Indexing
    ///////////////////////////////////////////////////////////////////////////

    /**
     * @brief Access the element at a multi-dimensional index, arr(i,j,k,...)
     *
     * @note The fast index is the last index. Please take care when iterating
     * through the array.
     */
    template <typename... Ix>
    std::enable_if_t<sizeof...(Ix) == Ndim, T &> operator()(Ix... index) {
        return derived().data()[element_offset(derived().strides(), index...)];
    }

    /** @brief Const version of arr(i,j,k,...) */
    template <typename... Ix>
    std::enable_if_t<sizeof...(Ix) == Ndim, const T &>
    operator()(Ix... index) const {
        return derived().data()[element_offset(derived().strides(), index...)];
    }

    /**
     * @brief Index the data as a 1D array, ignoring the shape. To get a
     * certain pixel use the (i,j,k,...) operator instead.
     */
    T &operator[](ssize_t i) { return derived().data()[i]; }

    /** @brief Const version of the 1D index. */
    const T &operator[](ssize_t i) const { return derived().data()[i]; }

    ///////////////////////////////////////////////////////////////////////////
    // Elementwise math with a scalar
    ///////////////////////////////////////////////////////////////////////////

    /** @brief Add a scalar to all elements. */
    Derived &operator+=(const T &value) {
        return elementwise(value, std::plus<T>());
    }

    /** @brief Subtract a scalar from all elements. */
    Derived &operator-=(const T &value) {
        return elementwise(value, std::minus<T>());
    }

    /** @brief Multiply all elements with a scalar. */
    Derived &operator*=(const T &value) {
        return elementwise(value, std::multiplies<T>());
    }

    /** @brief Divide all elements by a scalar. */
    Derived &operator/=(const T &value) {
        return elementwise(value, std::divides<T>());
    }

    /**
     * @brief Bitwise AND all elements with a scalar mask. Used for example to
     * mask out gain bits for Jungfrau detectors.
     */
    Derived &operator&=(const T &mask) {
        return elementwise(mask, std::bit_and<T>());
    }

    ///////////////////////////////////////////////////////////////////////////
    // Elementwise math with an NDArray, NDView or expression of the same shape
    ///////////////////////////////////////////////////////////////////////////

    /**
     * @brief Add elementwise. The right hand side can be an NDArray, an
     * NDView or an expression like a * b, also with another element type. An
     * expression is evaluated element by element without a temporary array.
     * @throws std::runtime_error if the shapes differ
     */
    template <typename E> Derived &operator+=(const ArrayExpr<E, Ndim> &expr) {
        return elementwise(expr, std::plus<>());
    }

    /** @brief Subtract elementwise, see operator+= */
    template <typename E> Derived &operator-=(const ArrayExpr<E, Ndim> &expr) {
        return elementwise(expr, std::minus<>());
    }

    /** @brief Multiply elementwise, see operator+= */
    template <typename E> Derived &operator*=(const ArrayExpr<E, Ndim> &expr) {
        return elementwise(expr, std::multiplies<>());
    }

    /** @brief Divide elementwise, see operator+= */
    template <typename E> Derived &operator/=(const ArrayExpr<E, Ndim> &expr) {
        return elementwise(expr, std::divides<>());
    }

    ///////////////////////////////////////////////////////////////////////////
    // Comparison
    ///////////////////////////////////////////////////////////////////////////

    /** @brief True if the shapes match and all elements compare equal. */
    bool operator==(const Derived &other) const {
        if (derived().shape() != other.shape())
            return false;
        const T *data = derived().data();
        const T *other_data = other.data();
        for (size_t i = 0, n = derived().size(); i != n; ++i) {
            if (data[i] != other_data[i])
                return false;
        }
        return true;
    }

    bool operator!=(const Derived &other) const { return !(*this == other); }

  private:
    Derived &derived() { return static_cast<Derived &>(*this); }
    const Derived &derived() const {
        return static_cast<const Derived &>(*this);
    }

    /**
     * @brief Apply data[i] = op(data[i], value) to all elements.
     *
     * Pointer and size are copied to locals since a store to data[i] may
     * alias the members of Derived for some T (uint8_t, int64_t, ...). The
     * compiler then has to reload them every iteration, which blocks
     * vectorization.
     */
    template <typename Op> Derived &elementwise(T value, Op op) {
        T *data = derived().data();
        for (size_t i = 0, n = derived().size(); i < n; ++i) {
            data[i] = op(data[i], value);
        }
        return derived();
    }

    /** @brief Apply data[i] = op(data[i], expr[i]) to all elements. */
    template <typename E, typename Op>
    Derived &elementwise(const ArrayExpr<E, Ndim> &expr, Op op) {
        if (expr.shape() != derived().shape())
            throw std::runtime_error(
                LOCATION + "Shapes must match for elementwise operations");
        // Holds its data pointers by value, see operand()
        const auto e = operand(expr);
        T *data = derived().data();
        for (size_t i = 0, n = derived().size(); i < n; ++i) {
            data[i] = op(data[i], e[i]);
        }
        return derived();
    }
};

template <typename Op, typename A, typename B, ssize_t Ndim>
class ArrayBinaryOp : public ArrayExpr<ArrayBinaryOp<Op, A, B, Ndim>, Ndim> {
    A arr1_;
    B arr2_;

  public:
    ArrayBinaryOp(const A &arr1, const B &arr2) : arr1_(arr1), arr2_(arr2) {
        assert(arr1.shape() == arr2.shape());
    }
    auto operator[](size_t i) const { return Op{}(arr1_[i], arr2_[i]); }
    size_t size() const { return arr1_.size(); }
    std::array<ssize_t, Ndim> shape() const { return arr1_.shape(); }
};

/** @brief Scalar operand, takes size and shape from the expression it is
 * combined with. */
template <typename T, ssize_t Ndim>
class ArrayScalar : public ArrayExpr<ArrayScalar<T, Ndim>, Ndim> {
    T value_;
    size_t size_;
    std::array<ssize_t, Ndim> shape_;

  public:
    template <typename E>
    ArrayScalar(T value, const ArrayExpr<E, Ndim> &like)
        : value_(value), size_(like.size()), shape_(like.shape()) {}
    T operator[](size_t) const { return value_; }
    size_t size() const { return size_; }
    std::array<ssize_t, Ndim> shape() const { return shape_; }
};

// Builds the expression for expr op expr, expr op scalar and scalar op expr
template <typename Op, typename A, typename B, ssize_t Ndim>
auto binary_op(const ArrayExpr<A, Ndim> &lhs, const ArrayExpr<B, Ndim> &rhs) {
    auto a = operand(lhs);
    auto b = operand(rhs);
    return ArrayBinaryOp<Op, decltype(a), decltype(b), Ndim>(a, b);
}

template <typename Op, typename A, typename T, ssize_t Ndim,
          typename = std::enable_if_t<std::is_arithmetic_v<T>>>
auto binary_op(const ArrayExpr<A, Ndim> &lhs, T rhs) {
    return binary_op<Op>(lhs, ArrayScalar<T, Ndim>(rhs, lhs));
}

template <typename Op, typename T, typename B, ssize_t Ndim,
          typename = std::enable_if_t<std::is_arithmetic_v<T>>>
auto binary_op(T lhs, const ArrayExpr<B, Ndim> &rhs) {
    return binary_op<Op>(ArrayScalar<T, Ndim>(lhs, rhs), rhs);
}

// The operators only take part in overload resolution if binary_op accepts
// the operands, meaning at least one of them is an ArrayExpr
template <typename L, typename R>
auto operator+(const L &lhs, const R &rhs)
    -> decltype(binary_op<std::plus<>>(lhs, rhs)) {
    return binary_op<std::plus<>>(lhs, rhs);
}

template <typename L, typename R>
auto operator-(const L &lhs, const R &rhs)
    -> decltype(binary_op<std::minus<>>(lhs, rhs)) {
    return binary_op<std::minus<>>(lhs, rhs);
}

template <typename L, typename R>
auto operator*(const L &lhs, const R &rhs)
    -> decltype(binary_op<std::multiplies<>>(lhs, rhs)) {
    return binary_op<std::multiplies<>>(lhs, rhs);
}

template <typename L, typename R>
auto operator/(const L &lhs, const R &rhs)
    -> decltype(binary_op<std::divides<>>(lhs, rhs)) {
    return binary_op<std::divides<>>(lhs, rhs);
}

} // namespace aare