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
        .def(py::init<int, int, int, double, double, int, std::size_t>(),
             R"(
             Initialize a PixelHistogram.

             Args:
                 rows: Number of rows in the detector
                 cols: Number of columns in the detector
                 n_bins: Number of histogram bins
                 xmin: Minimum value for histogram range
                 xmax: Maximum value for histogram range
                 n_threads: Number of threads for parallel filling (default: 1)
                 max_pending: Maximum number of images that can be queued for
                     asynchronous filling before fill_async() applies
                     backpressure on the caller (default: 16)
             )",
             py::arg("rows"), py::arg("cols"), py::arg("n_bins"),
             py::arg("xmin"), py::arg("xmax"), py::arg("n_threads") = 1,
             py::arg("max_pending") = std::size_t{16})

        .def(
            "fill_async",
            [](PixelHistogram &self,
               py::array_t<PixelHistogram::AxisType, 0> image) {
                // Copy the numpy buffer into an owned NDArray while we
                // still hold the GIL so we don't depend on the array's
                // backing storage outliving this call.
                auto view = make_view_2d(image);
                NDArray<PixelHistogram::AxisType, 2> owned(view);
                // Release the GIL while enqueueing - fill_async can block
                // on backpressure when the queue is full.
                py::gil_scoped_release release;
                self.fill_async(std::move(owned));
            },
            R"(
             Submit an image for asynchronous filling.

             The image is copied into an internal buffer before this call
             returns, so the caller may mutate or free the numpy array
             immediately. The actual histogram update happens on a
             background thread. If the internal queue is full this call
             blocks (with the GIL released) until a slot becomes available.

             Args:
                 image: A 2D numpy array of pixel values (dtype: float32)
             )",
            py::arg("image").noconvert())

        .def("flush", &PixelHistogram::flush,
             R"(
             Block until all images submitted via fill_async() have been
             merged into the accumulators. Cheap when nothing is pending.
             )",
             py::call_guard<py::gil_scoped_release>())

        .def("pending", &PixelHistogram::pending,
             R"(
             Return the number of images either waiting in the queue or
             currently being processed by the background thread. Useful
             for monitoring/diagnostics.
             )")

        .def(
            "values",
            [](const PixelHistogram &self) {
                // values() implicitly flushes - release the GIL while it
                // does so. Allocation/copy into the NDArray runs without
                // the GIL too; only the numpy wrapping needs it.
                NDArray<PixelHistogram::StorageType, 3> *ptr = nullptr;
                {
                    py::gil_scoped_release release;
                    ptr = new NDArray<PixelHistogram::StorageType, 3>(
                        self.values());
                }
                return return_image_data(ptr);
            },
            R"(
             Get the histogram data as a numpy array.

             Implicitly flushes any pending asynchronous fills before
             returning, so the snapshot is consistent with everything
             submitted up to this call.

             Returns:
                 A 3D numpy array containing the histogram bins for each pixel
             )")

        .def(
            "bin_centers",
            [](const PixelHistogram &self) {
                auto ptr = new NDArray<PixelHistogram::AxisType, 1>(
                    self.bin_centers());
                return return_image_data(ptr);
            },
            R"(
             Get the bin centers along the value axis.

             Returns:
                 A 1D numpy array containing the center values for each histogram bin
             )")
        .def(
            "bin_edges",
            [](const PixelHistogram &self) {
                auto ptr =
                    new NDArray<PixelHistogram::AxisType, 1>(self.bin_edges());
                return return_image_data(ptr);
            },
            R"(
             Get the bin edges along the value axis.

             Returns:
                 A 1D numpy array containing the edge values for the histogram bins
             )");
}
