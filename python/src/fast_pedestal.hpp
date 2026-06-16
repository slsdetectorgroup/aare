// SPDX-License-Identifier: MPL-2.0

#include "aare/FastPedestal.hpp"
#include "np_helper.hpp"

#include <cstdint>
#include <pybind11/numpy.h>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

namespace py = pybind11;

template <typename SUM_TYPE>
void define_fast_pedestal_bindings(py::module &m, const std::string &name) {

    py::class_<FastPedestal<SUM_TYPE>>(m, name.c_str(), py::buffer_protocol())
        .def(py::init<int, int, int>())
        .def(py::init<int, int>())
        .def("mean",
             [](FastPedestal<SUM_TYPE> &self) {
                 auto mean = new NDArray<SUM_TYPE, 2>{};
                 *mean = self.mean();
                 return return_image_data(mean);
             })
        .def("view",
             [](py::object self_py) {
                 auto &self = self_py.cast<FastPedestal<SUM_TYPE> &>();
                 auto view = self.view();
                 std::array<py::ssize_t, 2> shape{
                     static_cast<py::ssize_t>(view.shape(0)),
                     static_cast<py::ssize_t>(view.shape(1))};
                 std::array<py::ssize_t, 2> byte_strides{
                     static_cast<py::ssize_t>(view.strides()[0]) *
                         static_cast<py::ssize_t>(sizeof(SUM_TYPE)),
                     static_cast<py::ssize_t>(view.strides()[1]) *
                         static_cast<py::ssize_t>(sizeof(SUM_TYPE))};
                 auto array = py::array_t<SUM_TYPE>(shape, byte_strides,
                                                    view.data(), self_py);
                 array.attr("setflags")(py::arg("write") = false);
                 return array;
             })
        .def("variance",
             [](FastPedestal<SUM_TYPE> &self) {
                 auto variance = new NDArray<SUM_TYPE, 2>{};
                 *variance = self.variance();
                 return return_image_data(variance);
             })
        .def("std",
             [](FastPedestal<SUM_TYPE> &self) {
                 auto standard_deviation = new NDArray<SUM_TYPE, 2>{};
                 *standard_deviation = self.std();
                 return return_image_data(standard_deviation);
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
            "Support subtracting a FastPedestal from a NumPy array.")
        .def("clear", py::overload_cast<>(&FastPedestal<SUM_TYPE>::clear))
        .def_property_readonly("rows", &FastPedestal<SUM_TYPE>::rows)
        .def_property_readonly("cols", &FastPedestal<SUM_TYPE>::cols)
        .def_property_readonly("cur_samples",
                               &FastPedestal<SUM_TYPE>::cur_samples)
        .def_property_readonly("ready", &FastPedestal<SUM_TYPE>::ready)
        .def_property_readonly("n_samples", &FastPedestal<SUM_TYPE>::n_samples)
        // .def_property_readonly("sum", &FastPedestal<SUM_TYPE>::get_sum)
        .def("clone",
             [](FastPedestal<SUM_TYPE> &pedestal) {
                 return FastPedestal<SUM_TYPE>(pedestal);
             })
        .def(
            "push",
            [](FastPedestal<SUM_TYPE> &pedestal,
               py::array_t<uint16_t, py::array::c_style> &frame) {
                pedestal.push(make_view_2d(frame));
            },
            py::arg("frame").noconvert())
        .def(
            "push_init",
            [](FastPedestal<SUM_TYPE> &pedestal,
               py::array_t<uint16_t, py::array::c_style> &frame) {
                pedestal.push_init(make_view_2d(frame));
            },
            py::arg("frame").noconvert())
        // .def(
        //     "push_no_update",
        //     [](FastPedestal<SUM_TYPE> &pedestal,
        //        py::array_t<uint16_t, py::array::c_style> &frame) {
        //         pedestal.push_no_update(make_view_2d(frame));
        //     },
        //     py::arg("frame").noconvert())
        .def("update_mean", &FastPedestal<SUM_TYPE>::update_mean)
        .def_buffer([](FastPedestal<SUM_TYPE> &self) {
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
