// SPDX-License-Identifier: MPL-2.0
#pragma once
#include "aare/NDArray.hpp"
#include "aare/NDView.hpp"
#include "aare/hist/PedestalTrackingPixelHistogramCUDA.hpp"
#include "bind_PixelHistogramCUDA.hpp"
#include "np_helper.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <pybind11/numpy.h>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include <string>

namespace py = pybind11;

namespace aare {

template <typename StorageType>
void define_PedestalTrackingPixelHistogramCUDA(py::module &m,
                                               const std::string &class_name) {
    using Hist = cuda::PedestalTrackingPixelHistogramCUDA<StorageType>;
    using AxisType = typename Hist::AxisType;
    using FrameType = typename Hist::FrameType;
    constexpr const char *name = "PedestalTrackingPixelHistogramCUDA";

    py::class_<Hist> cls(m, class_name.c_str(),
                         R"(Per-pixel histograms of pedestal-subtracted frames,
             with the pedestal tracked during the fill, on the GPU.

             Not thread-safe. Build instances with the
             aare.PedestalTrackingPixelHistogramCUDA() factory rather than by
             class name.)");
    cls.def(
        py::init([](size_t rows, size_t cols, size_t n_bins, AxisType xmin,
                    AxisType xmax, AxisType n_sigma,
                    std::optional<py::array> mask, uint32_t n_samples,
                    size_t batch_size, bool window) {
            if (!mask)
                return new Hist(rows, cols, n_bins, xmin, xmax, n_sigma,
                                std::nullopt, n_samples, batch_size, window);
            if (!py::isinstance<py::array_t<bool>>(*mask))
                throw py::type_error(
                    "mask must have dtype bool, got " +
                    py::str(mask->dtype()).cast<std::string>() + ".");
            auto arr =
                py::array_t<bool, py::array::c_style | py::array::forcecast>(
                    *mask);
            // The constructor copies the mask.
            return new Hist(rows, cols, n_bins, xmin, xmax, n_sigma,
                            make_const_view_2d(arr), n_samples, batch_size,
                            window);
        }),
        py::kw_only(), py::arg("rows"), py::arg("cols"), py::arg("n_bins"),
        py::arg("xmin"), py::arg("xmax"), py::arg("n_sigma") = 1.0f,
        py::arg("mask") = py::none(), py::arg("n_samples") = 1000,
        py::arg("batch_size") = 1024, py::arg("window") = true);

    define_HistogramPipeline_methods<Hist, StorageType>(
        cls, name, /*dtype_selectable=*/false);

    cls.def(
           "push_pedestal_no_update",
           [name](Hist &self, const py::array &frames) {
               auto arr = detail::require_dtype<FrameType>(frames, name, false);
               if (arr.ndim() == 2) {
                   auto view = make_const_view_2d(arr);
                   py::gil_scoped_release release;
                   self.push_pedestal_no_update(view);
                   return;
               }
               auto view = make_const_view_3d(arr);
               py::gil_scoped_release release;
               for (ssize_t i = 0; i < view.shape(0); ++i)
                   self.push_pedestal_no_update(view.sub_view(i));
           },
           py::arg("frames"),
           R"(Add raw frames to the pedestal seed: one frame (rows, cols) or
             several (n, rows, cols), uint16.

             The seed needs exactly n_samples frames; the last one uploads the
             pedestal to the GPU. Raises RuntimeError for more.)")
        .def("update_mean", &Hist::update_mean,
             py::call_guard<py::gil_scoped_release>(),
             R"(Set each pixel's sigma from its pedestal as tracked so far.

             The fill gates the pedestal update with it until the next call.
             Call it once after the seed, and again whenever sigma should
             follow the tracked pedestal. Raises RuntimeError before the
             seed is complete.)")
        .def(
            "pedestal_mean",
            [](const Hist &self) {
                NDArray<AxisType, 2> *ptr = nullptr;
                {
                    py::gil_scoped_release release;
                    ptr = new NDArray<AxisType, 2>(self.pedestal_mean());
                }
                return return_image_data(ptr);
            },
            R"(Pedestal mean of every pixel, (rows, cols) float32, after
             every queued frame. Raises RuntimeError before the seed is
             complete.)")
        .def(
            "pedestal_std",
            [](const Hist &self) {
                NDArray<AxisType, 2> *ptr = nullptr;
                {
                    py::gil_scoped_release release;
                    ptr = new NDArray<AxisType, 2>(self.pedestal_std());
                }
                return return_image_data(ptr);
            },
            R"(Sigma of every pixel as set by the last update_mean(),
             (rows, cols) float32; 0 before.)")
        .def_property("n_sigma", &Hist::n_sigma, &Hist::set_n_sigma,
                      "Gate of the pedestal update: a value updates its "
                      "pixel's pedestal if abs(residual) < n_sigma * sigma. "
                      "0 or less: no tracking. A change applies to the frames "
                      "queued after it.")
        .def_property_readonly("n_samples", &Hist::n_samples,
                               "Seed frames needed, and the weight "
                               "1 / n_samples of a value in the update.")
        .def_property_readonly("pedestal_frames", &Hist::pedestal_frames,
                               "Seed frames pushed so far.");
}

// Class names are PedestalTrackingPixelHistogramCUDA_<storage>, e.g.
// PedestalTrackingPixelHistogramCUDA_u16 for uint16 counts.
inline void define_PedestalTrackingPixelHistogramCUDA_bindings(py::module &m) {
    define_PedestalTrackingPixelHistogramCUDA<uint16_t>(
        m, "PedestalTrackingPixelHistogramCUDA_u16");
    define_PedestalTrackingPixelHistogramCUDA<uint32_t>(
        m, "PedestalTrackingPixelHistogramCUDA_u32");
}

} // namespace aare
