// SPDX-License-Identifier: MPL-2.0
#include "aare/PixelHistogram.hpp"
#include "np_helper.hpp"

#include <cstdint>
#include <pybind11/numpy.h>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

namespace py = pybind11;
using namespace ::aare;

void define_pixel_histogram_bindings(py::module &m) {
    py::class_<PixelHistogram>(m, "PixelHistogram",
                                "A histogram for pixel-wise statistics")
        .def(py::init<int, int, int, double, double>(),
             R"(
             Initialize a PixelHistogram.
             
             Args:
                 rows: Number of rows in the detector
                 cols: Number of columns in the detector
                 n_bins: Number of histogram bins
                 xmin: Minimum value for histogram range
                 xmax: Maximum value for histogram range
             )",
             py::arg("rows"), py::arg("cols"), py::arg("n_bins"),
             py::arg("xmin"), py::arg("xmax"))

        .def("fill",
             [](PixelHistogram &self,
                py::array_t<double, py::array::forcecast> image) {
                 auto view = make_view_2d(image);
                 self.fill(view);
             },
             R"(
             Fill the histogram with image data.
             
             Args:
                 image: A 2D numpy array of pixel values (dtype: float64)
             )",
             py::arg("image"))

        .def("hdata",
             [](const PixelHistogram &self) {
                 auto ptr = new NDArray<PixelHistogram::StorageType, 3>(self.hdata());
                 return return_image_data(ptr);
             },
             R"(
             Get the histogram data as a numpy array.
             
             Returns:
                 A 3D numpy array containing the histogram bins for each pixel
             )")

        .def("bin_centers",
             [](const PixelHistogram &self) {
                 auto ptr = new NDArray<PixelHistogram::AxisType, 1>(self.bin_centers());
                 return return_image_data(ptr);
             },
             R"(
             Get the bin centers along the value axis.
             
             Returns:
                 A 1D numpy array containing the center values for each histogram bin
             )")
        .def("bin_edges",
             [](const PixelHistogram &self) {
                 auto ptr = new NDArray<PixelHistogram::AxisType, 1>(self.bin_edges());
                 return return_image_data(ptr);
             },
             R"(
             Get the bin edges along the value axis.
             
             Returns:
                 A 1D numpy array containing the edge values for the histogram bins
             )");
}
