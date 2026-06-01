// SPDX-License-Identifier: MPL-2.0
#include "aare/PedestalTrackingPixelHistogram.hpp"
#include "np_helper.hpp"

#include <cstdint>
#include <pybind11/numpy.h>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

namespace py = pybind11;
using namespace ::aare;

void define_pedestal_tracking_pixel_histogram_bindings(py::module &m) {
    py::class_<PedestalTrackingPixelHistogram>(
        m, "PedestalTrackingPixelHistogram",
        "A pixel-wise histogram of frame - pedestal residuals, with a "
        "per-pixel running pedestal estimate sharded across worker threads")
        .def(py::init<int, int, int, double, double, int, std::size_t,
                      double>(),
             R"(
             Initialize a PedestalTrackingPixelHistogram.

             Args:
                 rows: Number of rows in the detector
                 cols: Number of columns in the detector
                 n_bins: Number of histogram bins along the residual axis
                 xmin: Minimum residual value (inclusive)
                 xmax: Maximum residual value (exclusive)
                 n_threads: Number of worker threads (default: 1). Each
                     worker owns a disjoint row slice of both the
                     pedestal and the histogram, so the partition
                     determines per-thread memory usage.
                 max_pending: Maximum number of frames that can be
                     queued for asynchronous filling before
                     fill_async() applies backpressure
                     on the caller (default: 16).
                 n_sigma: Sigma multiplier used as the gate for the
                     pedestal-update side effect of
                     fill_async(): a pixel sample is
                     pushed back into the pedestal estimate iff
                     ``abs(residual) < n_sigma * cached_std``. Set to
                     ``0.0`` to disable the pedestal update and get
                     histogram-only async behaviour (default: 1.0).
                     Also exposed live via the ``n_sigma`` property.
             )",
             py::arg("rows"), py::arg("cols"), py::arg("n_bins"),
             py::arg("xmin"), py::arg("xmax"), py::arg("n_threads") = 1,
             py::arg("max_pending") = std::size_t{16},
             py::arg("n_sigma") = 1.0)

        .def("push_pedestal_no_update",
             [](PedestalTrackingPixelHistogram &self,
                py::array_t<PedestalTrackingPixelHistogram::FrameType, 0>
                    frame) {
                 auto view = make_view_2d(frame);
                 self.push_pedestal_no_update(view);
             },
             R"(
             Accumulate `frame` into the per-pixel running pedestal
             estimate without refreshing the cached mean.

             Use repeatedly while bootstrapping the pedestal, then call
             update_mean() once before starting to fill the histogram.

             Args:
                 frame: A 2D numpy array of raw pixel values (dtype: uint16)
             )",
             py::arg("frame").noconvert())

        .def("update_mean", &PedestalTrackingPixelHistogram::update_mean,
             R"(
             Refresh each partial pedestal's cached per-pixel mean from
             its running sums. Drains pending async fills first, then
             dispatches the update to the worker pool so the writes to
             each shard happen on the same thread that reads them in
             fill_async().
             )",
             py::call_guard<py::gil_scoped_release>())

        .def("pedestal_mean",
             [](const PedestalTrackingPixelHistogram &self) {
                 // pedestal_mean() flushes + locks + memcpys; do all of
                 // that without the GIL, only reacquire to wrap into a
                 // numpy array.
                 NDArray<PedestalTrackingPixelHistogram::AxisType, 2> *ptr = nullptr;
                 {
                     py::gil_scoped_release release;
                     ptr = new NDArray<PedestalTrackingPixelHistogram::AxisType, 2>(self.pedestal_mean());
                 }
                 return return_image_data(ptr);
             },
             R"(
             Snapshot the per-pixel pedestal mean stitched together
             from all shards.

             Returns:
                 A 2D numpy array (rows x cols, dtype: float64)
                 containing the current cached pedestal mean.
             )")

        .def("fill_async",
             [](PedestalTrackingPixelHistogram &self,
                py::array_t<PedestalTrackingPixelHistogram::FrameType, 0>
                    image) {
                 // Copy the numpy buffer into an owned NDArray while we
                 // still hold the GIL so we don't depend on the array's
                 // backing storage outliving this call.
                 auto view = make_view_2d(image);
                 NDArray<PedestalTrackingPixelHistogram::FrameType, 2> owned(
                     view);
                 // Release the GIL while enqueueing -
                 // fill_async can block on backpressure
                 // when the queue is full.
                 py::gil_scoped_release release;
                 self.fill_async(std::move(owned));
             },
             R"(
             Submit an image for asynchronous filling with sigma-clipped
             pedestal tracking.

             For each pixel the worker pool:
               * histograms the pedestal-subtracted residual when it
                 falls in ``[xmin, xmax)``, and
               * additionally pushes the raw pixel value back into the
                 per-thread pedestal estimate when
                 ``abs(residual) < n_sigma * cached_std`` (the
                 sigma-clipped pedestal-update gate).

             The cached std is populated by ``update_mean()``, so
             ``push_pedestal_no_update()`` + ``update_mean()`` must have
             run at least once for the pedestal-update side effect to
             fire. Setting ``n_sigma = 0`` disables the side effect and
             recovers plain histogram-only async filling.

             The image is copied into an internal buffer before this call
             returns, so the caller may mutate or free the numpy array
             immediately. If the internal queue is full this call blocks
             (with the GIL released) until a slot becomes available.

             Args:
                 image: A 2D numpy array of raw pixel values (dtype: uint16)
             )",
             py::arg("image").noconvert())

        .def_property("n_sigma", &PedestalTrackingPixelHistogram::n_sigma,
                      &PedestalTrackingPixelHistogram::set_n_sigma,
                      R"(
             Sigma multiplier used as the pedestal-update gate in
             fill_async(). Atomic; safe to read or write
             from any thread. Setting it to 0.0 disables the pedestal
             update entirely. The new value takes effect on subsequent
             per-pixel evaluations inside the worker pool.
             )")

        .def("flush", &PedestalTrackingPixelHistogram::flush,
             R"(
             Block until all images submitted via
             fill_async() have been merged into the
             accumulators. Cheap when nothing is pending.
             )",
             py::call_guard<py::gil_scoped_release>())

        .def("pending", &PedestalTrackingPixelHistogram::pending,
             R"(
             Return the number of images either waiting in the queue or
             currently being processed by the background thread (i.e.
             still in flight after fill_async()). Useful
             for monitoring/diagnostics.
             )")

        .def("values",
             [](const PedestalTrackingPixelHistogram &self) {
                 // values() implicitly flushes - release the GIL while it
                 // does so. Allocation/copy into the NDArray runs without
                 // the GIL too; only the numpy wrapping needs it.
                 NDArray<PedestalTrackingPixelHistogram::StorageType, 3>
                     *ptr = nullptr;
                 {
                     py::gil_scoped_release release;
                     ptr = new NDArray<
                         PedestalTrackingPixelHistogram::StorageType, 3>(
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
                 A 3D numpy array (rows x cols x n_bins, dtype: uint16)
                 containing the histogram bins for each pixel.
             )")

        .def("bin_centers",
             [](const PedestalTrackingPixelHistogram &self) {
                 auto ptr = new NDArray<
                     PedestalTrackingPixelHistogram::AxisType, 1>(
                     self.bin_centers());
                 return return_image_data(ptr);
             },
             R"(
             Get the bin centers along the residual axis.

             Returns:
                 A 1D numpy array (dtype: float32) of bin center values.
             )")

        .def("bin_edges",
             [](const PedestalTrackingPixelHistogram &self) {
                 auto ptr = new NDArray<
                     PedestalTrackingPixelHistogram::AxisType, 1>(
                     self.bin_edges());
                 return return_image_data(ptr);
             },
             R"(
             Get the bin edges along the residual axis.

             Returns:
                 A 1D numpy array (dtype: float32) of bin edge values.
             )");
}
