// SPDX-License-Identifier: MPL-2.0
#pragma once
#include "aare/defs.hpp"
#include <array>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <type_traits>

namespace aare {

template <ssize_t Dim = 0, typename Strides>
ssize_t element_offset(const Strides & /*unused*/) {
    return 0;
}

template <ssize_t Dim = 0, typename Strides, typename... Ix>
ssize_t element_offset(const Strides &strides, ssize_t i, Ix... index) {
    return i * strides[Dim] + element_offset<Dim + 1>(strides, index...);
}

template <typename Derived, typename T, ssize_t Ndim>
class NDIndexOps {
  public:
    template <typename... Ix>
    std::enable_if_t<sizeof...(Ix) == Ndim, T &> operator()(Ix... index) {
        return derived().data()[element_offset(derived().strides(), index...)];
    }

    template <typename... Ix>
    std::enable_if_t<sizeof...(Ix) == Ndim, const T &> operator()(Ix... index) const {
        return derived().data()[element_offset(derived().strides(), index...)];
    }

    T &operator()(ssize_t i) {
        return derived().data()[i];
    }
    
    const T &operator()(ssize_t i) const {
        return derived().data()[i];
    }

    T &operator[](ssize_t i) { return derived().data()[i]; }
    const T &operator[](ssize_t i) const { return derived().data()[i]; }

  private:
    Derived &derived() { return static_cast<Derived &>(*this); }
    const Derived &derived() const { return static_cast<const Derived &>(*this); }
};

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

template <typename A, typename B, ssize_t Ndim>
class ArrayAdd : public ArrayExpr<ArrayAdd<A, B, Ndim>, Ndim> {
    const A &arr1_;
    const B &arr2_;

  public:
    ArrayAdd(const A &arr1, const B &arr2) : arr1_(arr1), arr2_(arr2) {
        assert(arr1.size() == arr2.size());
    }
    auto operator[](int i) const { return arr1_[i] + arr2_[i]; }
    size_t size() const { return arr1_.size(); }
    std::array<ssize_t, Ndim> shape() const { return arr1_.shape(); }
};

template <typename A, typename B, ssize_t Ndim>
class ArraySub : public ArrayExpr<ArraySub<A, B, Ndim>, Ndim> {
    const A &arr1_;
    const B &arr2_;

  public:
    ArraySub(const A &arr1, const B &arr2) : arr1_(arr1), arr2_(arr2) {
        assert(arr1.size() == arr2.size());
    }
    auto operator[](int i) const { return arr1_[i] - arr2_[i]; }
    size_t size() const { return arr1_.size(); }
    std::array<ssize_t, Ndim> shape() const { return arr1_.shape(); }
};

template <typename A, typename B, ssize_t Ndim>
class ArrayMul : public ArrayExpr<ArrayMul<A, B, Ndim>, Ndim> {
    const A &arr1_;
    const B &arr2_;

  public:
    ArrayMul(const A &arr1, const B &arr2) : arr1_(arr1), arr2_(arr2) {
        assert(arr1.size() == arr2.size());
    }
    auto operator[](int i) const { return arr1_[i] * arr2_[i]; }
    size_t size() const { return arr1_.size(); }
    std::array<ssize_t, Ndim> shape() const { return arr1_.shape(); }
};

template <typename A, typename B, ssize_t Ndim>
class ArrayDiv : public ArrayExpr<ArrayDiv<A, B, Ndim>, Ndim> {
    const A &arr1_;
    const B &arr2_;

  public:
    ArrayDiv(const A &arr1, const B &arr2) : arr1_(arr1), arr2_(arr2) {
        assert(arr1.size() == arr2.size());
    }
    auto operator[](int i) const { return arr1_[i] / arr2_[i]; }
    size_t size() const { return arr1_.size(); }
    std::array<ssize_t, Ndim> shape() const { return arr1_.shape(); }
};

template <typename A, typename B, ssize_t Ndim>
auto operator+(const ArrayExpr<A, Ndim> &arr1, const ArrayExpr<B, Ndim> &arr2) {
    return ArrayAdd<ArrayExpr<A, Ndim>, ArrayExpr<B, Ndim>, Ndim>(arr1, arr2);
}

template <typename A, typename B, ssize_t Ndim>
auto operator-(const ArrayExpr<A, Ndim> &arr1, const ArrayExpr<B, Ndim> &arr2) {
    return ArraySub<ArrayExpr<A, Ndim>, ArrayExpr<B, Ndim>, Ndim>(arr1, arr2);
}

template <typename A, typename B, ssize_t Ndim>
auto operator*(const ArrayExpr<A, Ndim> &arr1, const ArrayExpr<B, Ndim> &arr2) {
    return ArrayMul<ArrayExpr<A, Ndim>, ArrayExpr<B, Ndim>, Ndim>(arr1, arr2);
}

template <typename A, typename B, ssize_t Ndim>
auto operator/(const ArrayExpr<A, Ndim> &arr1, const ArrayExpr<B, Ndim> &arr2) {
    return ArrayDiv<ArrayExpr<A, Ndim>, ArrayExpr<B, Ndim>, Ndim>(arr1, arr2);
}

} // namespace aare
