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

    py::class_<FastPedestal<SUM_TYPE>>(
        m, name.c_str(),
        "Maintain a per-pixel running mean, population variance, and "
        "standard deviation.",
        py::buffer_protocol())
        .def(py::init<uint32_t, uint32_t, uint32_t>(), py::arg("rows"),
             py::arg("cols"), py::arg("n_samples"),
             "Construct an empty pedestal. It becomes ready after n_samples "
             "calls to add_init_frame().")
        .def(py::init<uint32_t, uint32_t>(), py::arg("rows"), py::arg("cols"),
             "Construct an empty pedestal with n_samples=1000.")

        .def(
            "mean",
            [](FastPedestal<SUM_TYPE> &self) {
                auto mean = new NDArray<SUM_TYPE, 2>{};
                *mean = self.mean();
                return return_image_data(mean);
            },
            "Return a copy of the cached mean. The pedestal must be ready.")
        .def(
            "var",
            [](FastPedestal<SUM_TYPE> &self) {
                auto variance = new NDArray<SUM_TYPE, 2>{};
                *variance = self.variance();
                return return_image_data(variance);
            },
            "Return the population variance, normalized by n_samples, as a "
            "NumPy array. The pedestal must be ready.")
        .def(
            "std",
            [](FastPedestal<SUM_TYPE> &self) {
                auto standard_deviation = new NDArray<SUM_TYPE, 2>{};
                *standard_deviation = self.std();
                return return_image_data(standard_deviation);
            },
            "Return the population standard deviation as a NumPy array. The "
            "pedestal must be ready.")
        .def(
            "view",
            [](py::object self_py) {
                return py::module_::import("numpy").attr("asarray")(self_py);
            },
            "Return a non-owning, non-writable NumPy view of the cached mean. "
            "The pedestal must be ready.")

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
        .def("clear", py::overload_cast<>(&FastPedestal<SUM_TYPE>::clear),
             "Reset all statistics and initialization state to zero.")
        .def_property_readonly("rows", &FastPedestal<SUM_TYPE>::rows,
                               "Number of image rows.")
        .def_property_readonly("cols", &FastPedestal<SUM_TYPE>::cols,
                               "Number of image columns.")
        .def_property_readonly("cur_samples",
                               &FastPedestal<SUM_TYPE>::cur_samples,
                               "Number of initialization frames accumulated. "
                               "Steady-state pushes do not change it.")
        .def_property_readonly(
            "ready", &FastPedestal<SUM_TYPE>::ready,
            "Whether n_samples initialization frames have been accumulated.")
        .def_property_readonly(
            "n_samples", &FastPedestal<SUM_TYPE>::n_samples,
            "Initialization frame count and steady-state update-weight "
            "denominator.")
        .def(
            "clone",
            [](FastPedestal<SUM_TYPE> &pedestal) {
                return FastPedestal<SUM_TYPE>(pedestal);
            },
            "Return an independent copy of the pedestal and its state.")
        .def(
            "push_ema",
            [](FastPedestal<SUM_TYPE> &pedestal,
               py::array_t<uint16_t, py::array::c_style> &frame) {
                pedestal.push_ema(make_view_2d(frame));
            },
            py::arg("frame").noconvert(),
            "Apply a uint16 frame as a steady-state update. The pedestal must "
            "already be ready.")
        .def(
            "add_init_frame",
            [](FastPedestal<SUM_TYPE> &pedestal,
               py::array_t<uint16_t, py::array::c_style> &frame) {
                pedestal.add_init_frame(make_view_2d(frame));
            },
            py::arg("frame").noconvert(),
            "Accumulate one uint16 initialization frame. Call exactly "
            "n_samples times to make the pedestal ready.")
        .def_static(
            "from_file",
            [](const std::filesystem::path &filename, uint32_t n_samples,
               uint32_t skip_first) {
                return FastPedestal<SUM_TYPE>::template from_file<uint16_t>(
                    filename, n_samples, skip_first);
            },
            py::arg("filename"), py::arg("n_samples") = 1000,
            py::arg("skip_first") = 0,
            "Create a pedestal from a uint16 file. Skip skip_first frames, use "
            "the next n_samples for initialization, then apply every remaining "
            "frame as a steady-state update.");
}
