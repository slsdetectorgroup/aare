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

    py::class_<Pedestal<SUM_TYPE>>(
        m, name.c_str(),
        "Maintain a per-pixel running mean and population standard deviation. "
        "Statistics are available during initialization and are zero for empty "
        "pixels.",
        py::buffer_protocol())
        .def(py::init<uint32_t, uint32_t, uint32_t>(), py::arg("rows"),
             py::arg("cols"), py::arg("n_samples"),
             "Construct an empty pedestal. Each pixel accumulates n_samples "
             "values before switching to exponential updates.")
        .def(py::init<uint32_t, uint32_t>(), py::arg("rows"), py::arg("cols"),
             "Construct an empty pedestal with n_samples=1000.")
        .def(
            "mean",
            [](Pedestal<SUM_TYPE> &self) {
                auto mea = new NDArray<SUM_TYPE, 2>{};
                *mea = self.mean();
                return return_image_data(mea);
            },
            "Return a copy of the cached mean. Empty pixels return zero.")
        .def(
            "view",
            [](py::object self_py) {
                return py::module_::import("numpy").attr("asarray")(self_py);
            },
            "Return a non-owning, non-writable NumPy view of the cached mean.")
        .def(
            "std",
            [](Pedestal<SUM_TYPE> &self) {
                auto std = new NDArray<SUM_TYPE, 2>{};
                *std = self.std();
                return return_image_data(std);
            },
            "Return the population standard deviation as a NumPy array. "
            "Empty pixels return zero.")
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
        .def("clear", py::overload_cast<>(&Pedestal<SUM_TYPE>::clear),
             "Reset all statistics and per-pixel sample counts to zero.")
        .def_property_readonly("rows", &Pedestal<SUM_TYPE>::rows,
                               "Number of image rows.")
        .def_property_readonly("cols", &Pedestal<SUM_TYPE>::cols,
                               "Number of image columns.")
        .def_property_readonly(
            "n_samples", &Pedestal<SUM_TYPE>::n_samples,
            "Initialization sample count per pixel and steady-state "
            "update-weight denominator.")
        .def(
            "clone",
            [&](Pedestal<SUM_TYPE> &pedestal) {
                return Pedestal<SUM_TYPE>(pedestal);
            },
            "Return an independent copy of the pedestal and its state.")
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
            py::arg("frame").noconvert(),
            "Accumulate or exponentially update every pixel from a "
            "C-contiguous uint16 frame matching the pedestal shape. After "
            "n_samples values per pixel, new values have weight 1 / n_samples.")
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
            py::arg("frame").noconvert(), py::arg("threshold").noconvert(),
            "Push only pixels where abs(frame - mean) is strictly less than "
            "threshold. Both arrays must be C-contiguous with the pedestal "
            "shape; frame must be uint16 and threshold must use the output "
            "dtype. Rejected pixels keep their statistics and sample counts.")
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
