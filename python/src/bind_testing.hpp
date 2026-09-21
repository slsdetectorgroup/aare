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
}
