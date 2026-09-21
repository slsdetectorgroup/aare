// SPDX-License-Identifier: MPL-2.0
#pragma once
#include "aare/defs.hpp"
#include <array>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <functional>
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

// NDArray is held by reference. Sub expressions, scalars and views are
// temporaries and have to be held by value.
template <typename E>
using ExprStorage = std::conditional_t<E::is_leaf, const E &, E>;

template <typename Op, typename A, typename B, ssize_t Ndim>
class ArrayBinaryOp : public ArrayExpr<ArrayBinaryOp<Op, A, B, Ndim>, Ndim> {
    ExprStorage<A> arr1_;
    ExprStorage<B> arr2_;

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
    return ArrayBinaryOp<Op, A, B, Ndim>(static_cast<const A &>(lhs),
                                         static_cast<const B &>(rhs));
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