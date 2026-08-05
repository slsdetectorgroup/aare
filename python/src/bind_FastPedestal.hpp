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
        .def(py::init<uint32_t, uint32_t, uint32_t>())
        .def(py::init<uint32_t, uint32_t>())

        .def(
            "mean",
            [](FastPedestal<SUM_TYPE> &self) {
                auto mean = new NDArray<SUM_TYPE, 2>{};
                *mean = self.mean();
                return return_image_data(mean);
            },
            "Return a copy of the mean of the pedestal as a NumPy array")
        .def(
            "var",
            [](FastPedestal<SUM_TYPE> &self) {
                auto variance = new NDArray<SUM_TYPE, 2>{};
                *variance = self.variance();
                return return_image_data(variance);
            },
            "Return a copy of the variance of the pedestal as a NumPy array")
        .def(
            "std",
            [](FastPedestal<SUM_TYPE> &self) {
                auto standard_deviation = new NDArray<SUM_TYPE, 2>{};
                *standard_deviation = self.std();
                return return_image_data(standard_deviation);
            },
            "Return a copy of the standard deviation of the pedestal as a "
            "NumPy array")
        .def(
            "view",
            [](py::object self_py) {
                return py::module_::import("numpy").attr("asarray")(self_py);
            },
            "Return non-owning, non-writable view of the pedestal as a NumPy "
            "array")

        // We need to buffer protocol to allow for numpy operations using the
        // pedestal mean
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
        })
        // Subtracting a FastPedestal from a NumPy array
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
                               &FastPedestal<SUM_TYPE>::cur_samples,
                               "Return the number of samples pushed if < "
                               "n_samples in the pedestal")
        .def_property_readonly(
            "ready", &FastPedestal<SUM_TYPE>::ready,
            "Return true if the pedestal is ready to be used (i.e. we have "
            "pushed at least n_samples samples)")
        .def_property_readonly(
            "n_samples", &FastPedestal<SUM_TYPE>::n_samples,
            "Return the number of samples to push to the pedestal to be ready")
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
            py::arg("frame").noconvert(),
            "Push a frame to the pedestal to initialize it. Needs to be called "
            "n_samples times to be ready")
        .def_static(
            "from_file",
            [](const std::filesystem::path &filename, uint32_t n_samples,
               uint32_t skip_first) {
                return FastPedestal<SUM_TYPE>::template from_file<uint16_t>(
                    filename, n_samples, skip_first);
            },
            py::arg("filename"), py::arg("n_samples") = 1000,
            py::arg("skip_first") = 0,
            "Create a FastPedestal from a file. Uses n_samples frames for "
            "initialization (after skip_first), then pushes any remaining "
            "frames in steady state.");
}
