// SPDX-License-Identifier: MPL-2.0
#pragma once

#include <iostream>
#include <pybind11/numpy.h>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

#include "aare/Frame.hpp"
#include "aare/NDArray.hpp"
#include "aare/NDView.hpp"

namespace py = pybind11;
using namespace aare;

// Pass image data back to python as a numpy array
template <typename T, ssize_t Ndim>
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

// Create a NDView of a numpy array. NDView assumes C-order strides so we
// reject anything that is not Ndim dimensional and C-contiguous instead of
// silently reading the data in the wrong order.
template <ssize_t Ndim, class T, int Flags>
auto get_checked_shape(const py::array_t<T, Flags> &arr) {
    if (arr.ndim() != Ndim) {
        throw py::value_error(
            fmt::format("Expected {}D array, got {}D", Ndim, arr.ndim()));
    }
    if (!(arr.flags() & py::array::c_style)) {
        throw py::value_error(
            "Array is not C-contiguous. Use np.ascontiguousarray(arr)");
    }
    aare::Shape<Ndim> shape{};
    for (ssize_t i = 0; i < Ndim; ++i) {
        shape[i] = arr.shape(i);
    }
    return shape;
}

template <ssize_t Ndim, class T, int Flags>
auto make_view(py::array_t<T, Flags> &arr) {
    auto shape = get_checked_shape<Ndim>(arr);
    return aare::NDView<T, Ndim>(arr.mutable_data(), shape);
}

// Read only view, also works for numpy arrays that are not writeable
template <ssize_t Ndim, class T, int Flags>
auto make_const_view(const py::array_t<T, Flags> &arr) {
    auto shape = get_checked_shape<Ndim>(arr);
    return aare::NDView<const T, Ndim>(arr.data(), shape);
}

template <class T, int Flags> auto make_view_3d(py::array_t<T, Flags> &arr) {
    return make_view<3>(arr);
}
template <class T, int Flags> auto make_view_2d(py::array_t<T, Flags> &arr) {
    return make_view<2>(arr);
}
template <class T, int Flags> auto make_view_1d(py::array_t<T, Flags> &arr) {
    return make_view<1>(arr);
}

template <class T, int Flags>
auto make_const_view_3d(const py::array_t<T, Flags> &arr) {
    return make_const_view<3>(arr);
}
template <class T, int Flags>
auto make_const_view_2d(const py::array_t<T, Flags> &arr) {
    return make_const_view<2>(arr);
}
template <class T, int Flags>
auto make_const_view_1d(const py::array_t<T, Flags> &arr) {
    return make_const_view<1>(arr);
}

template <typename ClusterType> struct fmt_format_trait; // forward declaration

template <typename T, uint8_t ClusterSizeX, uint8_t ClusterSizeY,
          typename CoordType>
struct fmt_format_trait<Cluster<T, ClusterSizeX, ClusterSizeY, CoordType>> {

    static std::string value() {
        return fmt::format("T{{{}:x:{}:y:{}:data:}}",
                           py::format_descriptor<CoordType>::format(),
                           py::format_descriptor<CoordType>::format(),
                           fmt::format("({},{}){}", ClusterSizeX, ClusterSizeY,
                                       py::format_descriptor<T>::format()));
    }
};

template <typename ClusterType>
auto fmt_format = fmt_format_trait<ClusterType>::value();

/**
 * Helper function to allocate image data given item size and shape
 * used when we want to fill a numpy array and return to python
 */
py::array allocate_image_data(size_t item_size,
                              const std::vector<size_t> &shape) {
    py::array image_data;
    if (item_size == 1) {
        image_data = py::array_t<uint8_t>(shape);
    } else if (item_size == 2) {
        image_data = py::array_t<uint16_t>(shape);
    } else if (item_size == 4) {
        image_data = py::array_t<uint32_t>(shape);
    }
    return image_data;
}