// SPDX-License-Identifier: MPL-2.0
#pragma once
#include "aare/ClusterFinderCUDAOpt2.hpp"
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

// Binding for the OPT2 snapshot finder (pre-refactor pipeline: per-frame pinned
// staging, round-robin streams with sync barriers, variable-length D2H). Kept
// only for benchmarking the optimization arc; not part of the shipped API.
template <typename T, uint8_t ClusterSizeX, uint8_t ClusterSizeY,
          typename CoordType = uint16_t>
void define_ClusterFinderCUDAOpt2(py::module &m, const std::string &typestr) {
    auto class_name = fmt::format("ClusterFinderCUDAOpt2_{}", typestr);

    using ClusterType = Cluster<T, ClusterSizeX, ClusterSizeY, CoordType>;
    using CF = ClusterFinderCUDAOpt2<ClusterType, uint16_t, pd_type>;
    using ContigArr =
        py::array_t<uint16_t, py::array::c_style | py::array::forcecast>;

    py::class_<CF>(m, class_name.c_str())
        // ctor: (image_size, n_sigma, capacity, n_streams) — capacity is the
        // per-stream device cluster buffer (upper bound on clusters/frame).
        .def(py::init<Shape<2>, float, size_t, int>(), py::arg("image_size"),
             py::arg("n_sigma") = 5.0f,
             py::arg("max_clusters_per_frame") = 3000, py::arg("n_streams") = 4)

        .def_property(
            "nSigma", &CF::get_nSigma, &CF::set_nSigma,
            R"(Number of sigma above the pedestal to consider a photon.)")

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
                return self.steal_clusters(realloc_same_capacity);
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
            R"(Process a 3D array (n_frames, nrows, ncols) round-robin across
n_streams. Returns a list of ClusterVector, one per input frame.)")

        .def("avg_kernel_time_ms", &CF::avg_kernel_time_ms)
        .def("reset_timers", &CF::reset_timers);
}

} // namespace aare

#pragma GCC diagnostic pop
