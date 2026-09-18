// SPDX-License-Identifier: MPL-2.0
// Probes used by the python test-suite to exercise the helpers in
// np_helper.hpp. Internal, no API guarantees.
#pragma once

#include "aare/NDArray.hpp"
#include "aare/NDView.hpp"
#include "np_helper.hpp"

#include <algorithm>
#include <cstdint>
#include <pybind11/numpy.h>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include <type_traits>

namespace py = pybind11;
using namespace ::aare;

// Arrays are taken with the default flags (no c_style) so that nothing is
// made contiguous by pybind11 before it reaches make_view
template <typename T, ssize_t Ndim>
void define_make_view_probes(py::module &m, const std::string &suffix) {

    // Copy what C++ sees through the view back to python
    m.def(("view_roundtrip_" + suffix).c_str(),
          [](py::array_t<T> arr) {
              auto view = make_view<Ndim>(arr);
              return return_image_data(new NDArray<T, Ndim>(view));
          },
          py::arg("arr").noconvert());

    // shape, strides (in elements) and address of the data seen by the view
    m.def(("view_info_" + suffix).c_str(),
          [](py::array_t<T> arr) {
              auto view = make_view<Ndim>(arr);
              return py::make_tuple(view.shape(), view.strides(),
                                    reinterpret_cast<uintptr_t>(view.data()));
          },
          py::arg("arr").noconvert());

    // Same as above but through a read only view
    m.def(("const_view_roundtrip_" + suffix).c_str(),
          [](py::array_t<T> arr) {
              auto view = make_const_view<Ndim>(arr);
              static_assert(
                  std::is_same_v<decltype(view), NDView<const T, Ndim>>);
              auto out = new NDArray<T, Ndim>(view.shape());
              std::copy(view.begin(), view.end(), out->begin());
              return return_image_data(out);
          },
          py::arg("arr").noconvert());

    m.def(("const_view_info_" + suffix).c_str(),
          [](py::array_t<T> arr) {
              auto view = make_const_view<Ndim>(arr);
              return py::make_tuple(view.shape(), view.strides(),
                                    reinterpret_cast<uintptr_t>(view.data()));
          },
          py::arg("arr").noconvert());

    // Write through the view
    m.def(("view_fill_" + suffix).c_str(),
          [](py::array_t<T> arr, T value) {
              auto view = make_view<Ndim>(arr);
              view = value;
          },
          py::arg("arr").noconvert(), py::arg("value"));
}

void define_testing_bindings(py::module &m) {
    auto testing = m.def_submodule(
        "testing", "Internal helpers for the test-suite, no API guarantees");

    // noconvert + overloads gives dispatch on dtype
    define_make_view_probes<double, 1>(testing, "1d");
    define_make_view_probes<double, 2>(testing, "2d");
    define_make_view_probes<double, 3>(testing, "3d");
    define_make_view_probes<uint16_t, 1>(testing, "1d");
    define_make_view_probes<uint16_t, 2>(testing, "2d");
    define_make_view_probes<uint16_t, 3>(testing, "3d");

    // The wrappers used by the bindings
    testing.def(
        "make_view_1d_info",
        [](py::array_t<double> arr) {
            auto view = make_view_1d(arr);
            return py::make_tuple(view.shape(), view.strides());
        },
        py::arg("arr").noconvert());
    testing.def(
        "make_view_2d_info",
        [](py::array_t<double> arr) {
            auto view = make_view_2d(arr);
            return py::make_tuple(view.shape(), view.strides());
        },
        py::arg("arr").noconvert());
    testing.def(
        "make_view_3d_info",
        [](py::array_t<double> arr) {
            auto view = make_view_3d(arr);
            return py::make_tuple(view.shape(), view.strides());
        },
        py::arg("arr").noconvert());

    testing.def(
        "make_const_view_1d_info",
        [](py::array_t<double> arr) {
            auto view = make_const_view_1d(arr);
            return py::make_tuple(view.shape(), view.strides());
        },
        py::arg("arr").noconvert());
    testing.def(
        "make_const_view_2d_info",
        [](py::array_t<double> arr) {
            auto view = make_const_view_2d(arr);
            return py::make_tuple(view.shape(), view.strides());
        },
        py::arg("arr").noconvert());
    testing.def(
        "make_const_view_3d_info",
        [](py::array_t<double> arr) {
            auto view = make_const_view_3d(arr);
            return py::make_tuple(view.shape(), view.strides());
        },
        py::arg("arr").noconvert());

    // With c_style in the signature pybind11 hands us a contiguous temporary
    // so make_view accepts any layout, but writes don't reach the caller
    testing.def(
        "view_roundtrip_2d_c_style",
        [](py::array_t<double, py::array::c_style | py::array::forcecast> arr) {
            auto view = make_view_2d(arr);
            return return_image_data(new NDArray<double, 2>(view));
        },
        py::arg("arr"));
    testing.def(
        "view_fill_2d_c_style",
        [](py::array_t<double, py::array::c_style | py::array::forcecast> arr,
           double value) {
            auto view = make_view_2d(arr);
            view = value;
        },
        py::arg("arr"), py::arg("value"));
}
