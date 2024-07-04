#include <pybind11/numpy.h>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

#include "aare/core/Dtype.hpp"
#include "aare/core/NDArray.hpp"
#include "aare/core/NDView.hpp"

namespace py = pybind11;
using namespace aare;
using namespace std;

template <typename ArrayType, int64_t Ndim> void define_NDArray_bindings(py::module_ &m) {
    std::string name = "NDArray_" + Dtype(typeid(ArrayType)).to_string() + "_" + to_string(Ndim);

    py::class_<NDArray<ArrayType, Ndim>>(m, name.c_str(), py::buffer_protocol())
        .def(py::init<array<int64_t, Ndim>>())
        .def(py::init([](py::array_t<ArrayType, py::array::c_style | py::array::forcecast> &np_array) {
            py::buffer_info info = np_array.request();
            if (info.format != py::format_descriptor<ArrayType>::format())
                throw std::runtime_error(
                    "Incompatible format: different formats! (Are you sure the arrays are of the same type?)");
            if (info.ndim != Ndim)
                throw std::runtime_error("Incompatible dimension: expected a" + to_string(Ndim) + " array!");

            std::array<int64_t, Ndim> arr_shape;
            std::move(info.shape.begin(), info.shape.end(), arr_shape.begin());

            NDArray<ArrayType, Ndim> a(arr_shape);
            std::memcpy(a.data(), info.ptr, info.size * sizeof(ArrayType));
            return a;
        }))
        .def("__getitem__",
             [](NDArray<ArrayType, Ndim> &a, py::tuple index) {
                 if (index.size() != 2) {
                     throw std::runtime_error("Index must be a tuple of size " + to_string(Ndim));
                 }
                 auto offset = 0;
                 for (size_t i = 0; i < Ndim; i++) {
                     offset += index[i].cast<int64_t>() * a.strides()[i];
                 }
                 return a(offset);
             })

        .def_property_readonly("shape", [](NDArray<ArrayType, Ndim> &a) { return a.shape(); })
        .def_property_readonly("size", &NDArray<ArrayType, Ndim>::size)
        .def_property_readonly("bitdepth", &NDArray<ArrayType, Ndim>::bitdepth)
        .def_property_readonly("strides", &NDArray<ArrayType, Ndim>::strides)
        .def_property_readonly("byte_strides", &NDArray<ArrayType, Ndim>::byte_strides)
        .def("__add__", [](NDArray<ArrayType, Ndim> &a, NDArray<ArrayType, Ndim> &b) { return a + b; })
        .def_buffer([](NDArray<ArrayType, Ndim> &a) -> py::buffer_info {
            return py::buffer_info(
                a.data(),                                   /* Pointer to buffer */
                sizeof(ArrayType),                          /* Size of one scalar */
                py::format_descriptor<ArrayType>::format(), /* Python struct-style format descriptor */
                Ndim,                                       /* Number of dimensions */
                a.shape(),                                  /* Buffer dimensions */
                a.byte_strides()                            /* Strides (in bytes) for each index */
            );
        });
}
