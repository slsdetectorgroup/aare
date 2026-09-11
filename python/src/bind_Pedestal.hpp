// SPDX-License-Identifier: MPL-2.0

#include "aare/Pedestal.hpp"
#include "np_helper.hpp"

#include <cstdint>
#include <filesystem>
#include <pybind11/numpy.h>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

namespace py = pybind11;

template <typename SUM_TYPE>
void define_pedestal_bindings(py::module &m, const std::string &name) {

    py::class_<Pedestal<SUM_TYPE>>(m, name.c_str(), py::buffer_protocol())
        .def(py::init<uint32_t, uint32_t, uint32_t>(), py::arg("rows"),
             py::arg("cols"), py::arg("n_samples"))
        .def(py::init<uint32_t, uint32_t>(), py::arg("rows"), py::arg("cols"))
        .def("mean",
             [](Pedestal<SUM_TYPE> &self) {
                 auto mea = new NDArray<SUM_TYPE, 2>{};
                 *mea = self.mean();
                 return return_image_data(mea);
             })
        .def("view",
             [](py::object self_py) {
                 auto &self = self_py.cast<Pedestal<SUM_TYPE> &>();
                 auto v = self.view();
                 std::array<py::ssize_t, 2> shape{
                     static_cast<py::ssize_t>(v.shape(0)),
                     static_cast<py::ssize_t>(v.shape(1))};
                 std::array<py::ssize_t, 2> byte_strides{
                     static_cast<py::ssize_t>(v.strides()[0]) *
                         static_cast<py::ssize_t>(sizeof(SUM_TYPE)),
                     static_cast<py::ssize_t>(v.strides()[1]) *
                         static_cast<py::ssize_t>(sizeof(SUM_TYPE))};
                 auto arr = py::array_t<SUM_TYPE>(shape, byte_strides, v.data(),
                                                  self_py);
                 arr.attr("setflags")(py::arg("write") = false);
                 return arr;
             })
        .def("std",
             [](Pedestal<SUM_TYPE> &self) {
                 auto std = new NDArray<SUM_TYPE, 2>{};
                 *std = self.std();
                 return return_image_data(std);
             })
        .def(
            "__array_ufunc__",
            [](py::object self, py::object ufunc, const std::string &method,
               py::args inputs, py::kwargs kwargs) -> py::object {
                if (method != "__call__" || inputs.size() != 2 ||
                    inputs[1].ptr() != self.ptr() ||
                    py::cast<std::string>(ufunc.attr("__name__")) !=
                        "subtract") {
                    return py::reinterpret_borrow<py::object>(
                        Py_NotImplemented);
                }

                auto mean =
                    py::module_::import("builtins").attr("memoryview")(self);
                return ufunc(inputs[0], mean, **kwargs);
            },
            "Support subtracting a Pedestal from a NumPy array.")
        .def("clear", py::overload_cast<>(&Pedestal<SUM_TYPE>::clear))
        .def_property_readonly("rows", &Pedestal<SUM_TYPE>::rows)
        .def_property_readonly("cols", &Pedestal<SUM_TYPE>::cols)
        .def_property_readonly("n_samples", &Pedestal<SUM_TYPE>::n_samples)
        .def("clone",
             [&](Pedestal<SUM_TYPE> &pedestal) {
                 return Pedestal<SUM_TYPE>(pedestal);
             })
        // TODO! add push for other data types
        .def(
            "push",
            [](Pedestal<SUM_TYPE> &pedestal,
               py::array_t<uint16_t, py::array::c_style> &f) {
                if (f.ndim() != 2) {
                    throw py::value_error("Frame must be 2-dimensional");
                }
                auto v = make_view_2d(f);
                pedestal.push(v);
            },
            py::arg("frame").noconvert())
        .def(
            "push_with_threshold",
            [](Pedestal<SUM_TYPE> &pedestal,
               py::array_t<uint16_t, py::array::c_style> &f,
               py::array_t<SUM_TYPE, py::array::c_style> &threshold) {
                if (f.ndim() != 2) {
                    throw py::value_error("Frame must be 2-dimensional");
                }
                if (threshold.ndim() != 2) {
                    throw py::value_error("Threshold must be 2-dimensional");
                }
                auto frame_view = make_view_2d(f);
                auto threshold_view = make_view_2d(threshold);
                pedestal.push_with_threshold(frame_view, threshold_view);
            },
            py::arg("frame").noconvert(), py::arg("threshold").noconvert())
        .def_buffer([](Pedestal<SUM_TYPE> &self) {
            auto mean = self.view();
            return py::buffer_info(
                const_cast<SUM_TYPE *>(mean.data()), sizeof(SUM_TYPE),
                py::format_descriptor<SUM_TYPE>::format(), 2,
                {static_cast<py::ssize_t>(mean.shape(0)),
                 static_cast<py::ssize_t>(mean.shape(1))},
                {static_cast<py::ssize_t>(mean.strides()[0] * sizeof(SUM_TYPE)),
                 static_cast<py::ssize_t>(mean.strides()[1] *
                                          sizeof(SUM_TYPE))},
                true);
        });
}
