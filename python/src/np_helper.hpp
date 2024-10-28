#pragma once

#include <iostream>
#include <pybind11/numpy.h>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

#include "aare/Frame.hpp"
#include "aare/NDArray.hpp"

namespace py = pybind11;

// Pass image data back to python as a numpy array
// template <typename T, ssize_t Ndim>
// py::array return_image_data(pl::ImageData<T, Ndim> *image) {

//     py::capsule free_when_done(image, [](void *f) {
//         pl::ImageData<T, Ndim> *foo =
//             reinterpret_cast<pl::ImageData<T, Ndim> *>(f);
//         delete foo;
//     });

//     return py::array_t<T>(
//         image->shape(),        // shape
//         image->byte_strides(), // C-style contiguous strides for double
//         image->data(),         // the data pointer
//         free_when_done);       // numpy array references this parent
// }

// template <typename T> py::array return_vector(std::vector<T> *vec) {
//     py::capsule free_when_done(vec, [](void *f) {
//         std::vector<T> *foo = reinterpret_cast<std::vector<T> *>(f);
//         delete foo;
//     });
//     return py::array_t<T>({vec->size()}, // shape
//                           {sizeof(T)}, // C-style contiguous strides for double
//                           vec->data(), // the data pointer
//                           free_when_done); // numpy array references this parent
// }

// template <typename Reader> py::array do_read(Reader &r, size_t n_frames) {
//     py::array image;
//     if (n_frames == 0)
//         n_frames = r.total_frames();

//     std::array<ssize_t, 3> shape{static_cast<ssize_t>(n_frames), r.rows(),
//                                  r.cols()};
//     const uint8_t item_size = r.bytes_per_pixel();
//     if (item_size == 1) {
//         image = py::array_t<uint8_t, py::array::c_style | py::array::forcecast>(
//             shape);
//     } else if (item_size == 2) {
//         image =
//             py::array_t<uint16_t, py::array::c_style | py::array::forcecast>(
//                 shape);
//     } else if (item_size == 4) {
//         image =
//             py::array_t<uint32_t, py::array::c_style | py::array::forcecast>(
//                 shape);
//     }
//     r.read_into(reinterpret_cast<std::byte *>(image.mutable_data()), n_frames);
//     return image;
// }

py::array return_frame(pl::Frame *ptr) {
    py::capsule free_when_done(ptr, [](void *f) {
        pl::Frame *foo = reinterpret_cast<pl::Frame *>(f);
        delete foo;
    });

    const uint8_t item_size = ptr->bytes_per_pixel();
    std::vector<ssize_t> shape;
    for (auto val : ptr->shape())
        if (val > 1)
            shape.push_back(val);

    std::vector<ssize_t> strides;
    if (shape.size() == 1)
        strides.push_back(item_size);
    else if (shape.size() == 2) {
        strides.push_back(item_size * shape[1]);
        strides.push_back(item_size);
    }

    if (item_size == 1)
        return py::array_t<uint8_t>(
            shape, strides,
            reinterpret_cast<uint8_t *>(ptr->data()), free_when_done);
    else if (item_size == 2)
        return py::array_t<uint16_t>(shape, strides,
                                     reinterpret_cast<uint16_t *>(ptr->data()),
                                     free_when_done);
    else if (item_size == 4)
        return py::array_t<uint32_t>(shape, strides,
                                     reinterpret_cast<uint32_t *>(ptr->data()),
                                     free_when_done);
    return {};
}

// todo rewrite generic
template <class T, int Flags> auto get_shape_3d(py::array_t<T, Flags> arr) {
    return pl::Shape<3>{arr.shape(0), arr.shape(1), arr.shape(2)};
}

template <class T, int Flags> auto make_span_3d(py::array_t<T, Flags> arr) {
    return pl::DataSpan<T, 3>(arr.mutable_data(), get_shape_3d<T, Flags>(arr));
}

template <class T, int Flags> auto get_shape_2d(py::array_t<T, Flags> arr) {
    return pl::Shape<2>{arr.shape(0), arr.shape(1)};
}

template <class T, int Flags> auto make_span_2d(py::array_t<T, Flags> arr) {
    return pl::DataSpan<T, 2>(arr.mutable_data(), get_shape_2d<T, Flags>(arr));
}