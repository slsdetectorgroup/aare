#pragma once

#include <iostream>
#include <pybind11/numpy.h>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

#include "aare/Frame.hpp"
#include "aare/NDArray.hpp"
#include "aare/NDView.hpp"

namespace py = pybind11;

// Pass image data back to python as a numpy array
template <typename T, int64_t Ndim>
py::array return_image_data(aare::NDArray<T, Ndim> *image) {

    py::capsule free_when_done(image, [](void *f) {
        aare::NDArray<T, Ndim> *foo =
            reinterpret_cast<aare::NDArray<T, Ndim> *>(f);
        delete foo;
    });

    return py::array_t<T>(
        image->shape(),        // shape
        image->byte_strides(), // C-style contiguous strides for double
        image->data(),         // the data pointer
        free_when_done);       // numpy array references this parent
}

template <typename T> py::array return_vector(std::vector<T> *vec) {
    py::capsule free_when_done(vec, [](void *f) {
        std::vector<T> *foo = reinterpret_cast<std::vector<T> *>(f);
        delete foo;
    });
    return py::array_t<T>({vec->size()}, // shape
                          {sizeof(T)}, // C-style contiguous strides for double
                          vec->data(), // the data pointer
                          free_when_done); // numpy array references this parent
}

// todo rewrite generic
template <class T, int Flags> auto get_shape_3d(const py::array_t<T, Flags>& arr) {
    return aare::Shape<3>{arr.shape(0), arr.shape(1), arr.shape(2)};
}

template <class T, int Flags> auto make_view_3d(py::array_t<T, Flags>& arr) {
    return aare::NDView<T, 3>(arr.mutable_data(), get_shape_3d<T, Flags>(arr));
}

template <class T, int Flags> auto get_shape_2d(const py::array_t<T, Flags>& arr) {
    return aare::Shape<2>{arr.shape(0), arr.shape(1)};
}

template <class T, int Flags> auto get_shape_1d(const py::array_t<T, Flags>& arr) {
    return aare::Shape<1>{arr.shape(0)};
}

template <class T, int Flags> auto make_view_2d(py::array_t<T, Flags>& arr) {
    return aare::NDView<T, 2>(arr.mutable_data(), get_shape_2d<T, Flags>(arr));
}
template <class T, int Flags> auto make_view_1d(py::array_t<T, Flags>& arr) {
    return aare::NDView<T, 1>(arr.mutable_data(), get_shape_1d<T, Flags>(arr));
}