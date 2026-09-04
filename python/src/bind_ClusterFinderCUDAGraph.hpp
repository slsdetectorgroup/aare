// SPDX-License-Identifier: MPL-2.0
#pragma once
#include "aare/ClusterFinderCUDA_graph.hpp"
#include "aare/ClusterVector.hpp"
#include "aare/NDView.hpp"
#include "aare/Pedestal.hpp"
#include "np_helper.hpp"

#include <cstdint>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

namespace py = pybind11;
using pd_type = double;

using namespace aare;

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"

namespace aare {

template <typename T, uint8_t ClusterSizeX, uint8_t ClusterSizeY,
          typename CoordType = uint16_t>
void define_ClusterFinderCUDAGraph(py::module &m, const std::string &typestr) {
    auto class_name = fmt::format("ClusterFinderCUDAGraph_{}", typestr);

    using ClusterType = Cluster<T, ClusterSizeX, ClusterSizeY, CoordType>;
    using CF = ClusterFinderCUDAGraph<ClusterType, uint16_t, pd_type>;
    using ContigArr =
        py::array_t<uint16_t, py::array::c_style | py::array::forcecast>;

    py::class_<CF>(m, class_name.c_str())
        .def(py::init<Shape<2>, float, size_t, int>(), py::arg("image_size"),
             py::arg("n_sigma") = 5.0f,
             py::arg("max_clusters_per_frame") = 2048, py::arg("n_streams") = 4)

        .def_property(
            "nSigma", &CF::get_nSigma, &CF::set_nSigma,
            R"(Number of sigma above the pedestal to consider a photon during cluster finding.)")

        .def("push_pedestal_frame",
             [](CF &self, ContigArr frame) {
                 auto view = make_view_2d(frame);
                 self.push_pedestal_frame(view);
             })

        .def("clear_pedestal", &CF::clear_pedestal)

        .def_property_readonly("pedestal",
                               [](CF &self) {
                                   auto pd = new NDArray<pd_type, 2>{};
                                   *pd = self.pedestal();
                                   return return_image_data(pd);
                               })

        .def_property_readonly("noise",
                               [](CF &self) {
                                   auto arr = new NDArray<pd_type, 2>{};
                                   *arr = self.noise();
                                   return return_image_data(arr);
                               })

        .def(
            "steal_clusters",
            [](CF &self, bool realloc_same_capacity) {
                return std::move(self.steal_clusters(realloc_same_capacity));
            },
            py::arg("realloc_same_capacity") = true)

        .def(
            "find_clusters",
            [](CF &self, ContigArr frame, uint64_t frame_number) {
                auto view = make_view_2d(frame);
                self.find_clusters(view, frame_number);
            },
            py::arg("frame"), py::arg("frame_number") = 0,
            py::call_guard<py::gil_scoped_release>())

        .def(
            "find_clusters_batched",
            [](CF &self, ContigArr frames, uint64_t first_frame) {
                auto view = make_view_3d(frames);
                return self.find_clusters_batched(view, first_frame);
            },
            py::arg("frames"), py::arg("first_frame") = 0,
            py::call_guard<py::gil_scoped_release>(),
            R"(Process a 3D array of frames (n_frames, nrows, ncols) using
            n_streams CUDA Graphs for H2D/kernel/D2H pipelining. Returns a
            list of ClusterVector, one per input frame.)")

        .def(
            "avg_kernel_time_ms", &CF::avg_kernel_time_ms,
            R"(Always returns 0.0 — graph version does not instrument individual kernel time.
            Use wall-clock timing around find_clusters_batched instead.)")

        .def("reset_timers", &CF::reset_timers)

        .def(
            "register_input_buffer",
            [](CF &self, py::array arr) {
                auto info = arr.request();
                self.register_input_buffer(
                    info.ptr, static_cast<size_t>(info.size) *
                                  static_cast<size_t>(info.itemsize));
            },
            R"(Pin a numpy array as a locked host buffer so that
            find_clusters_batched transfers it at full DMA bandwidth.
            Call once before the processing loop with the full data array.
            Slices passed to find_clusters_batched lie within the registered
            region and benefit automatically. Call unregister_input_buffer()
            when done.)")

        .def("unregister_input_buffer", &CF::unregister_input_buffer,
             "Release the previously pinned input buffer.");
}

} // namespace aare

#pragma GCC diagnostic pop
