// SPDX-License-Identifier: MPL-2.0
#pragma once
#include "aare/ClusterFinderCUDA.hpp"
#include "aare/ClusterVector.hpp"
#include "aare/NDView.hpp"
#include "aare/Pedestal.hpp"
#include "np_helper.hpp"

#include <cstdint>
#include <pybind11/pybind11.h>
// #include <pybind11/stl.h>
#include <pybind11/stl_bind.h>

namespace py = pybind11;
using pd_type = double;

using namespace aare;

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"

namespace aare {

template <typename T, uint8_t ClusterSizeX, uint8_t ClusterSizeY,
          typename CoordType = uint16_t>
void define_ClusterFinderCUDA(py::module &m, const std::string &typestr) {
    auto class_name = fmt::format("ClusterFinderCUDA_{}", typestr);

    using ClusterType = Cluster<T, ClusterSizeX, ClusterSizeY, CoordType>;
    using CF          = ClusterFinderCUDA<ClusterType, uint16_t, pd_type>;

    py::class_<CF>(m, class_name.c_str())
        .def(py::init<Shape<2>, pd_type, size_t, int>(),
             py::arg("image_size"),
             py::arg("n_sigma")    = 5.0,
             py::arg("capacity")   = 1'000'000,
             py::arg("n_streams")  = 1)

        .def_property(
            "nSigma",
            &CF::get_nSigma,
            &CF::set_nSigma,
            R"(Number of sigma above the pedestal to consider a photon during cluster finding.)")

        .def("push_pedestal_frame",
             [](CF &self, py::array_t<uint16_t> frame) {
                 auto view = make_view_2d(frame);
                 self.push_pedestal_frame(view);
             })

        .def("clear_pedestal", &CF::clear_pedestal)

        .def_property_readonly(
            "pedestal",
            [](CF &self) {
                auto pd = new NDArray<pd_type, 2>{};
                *pd = self.pedestal();
                return return_image_data(pd);
            })

        .def_property_readonly(
            "noise",
            [](CF &self) {
                auto arr = new NDArray<pd_type, 2>{};
                *arr = self.noise();
                return return_image_data(arr);
            })

        .def(
            "steal_clusters",
            [](CF &self, bool realloc_same_capacity) {
                ClusterVector<ClusterType> clusters =
                    self.steal_clusters(realloc_same_capacity);
                return clusters;
            },
            py::arg("realloc_same_capacity") = false)

        .def(
            "find_clusters",
            [](CF &self, py::array_t<uint16_t> frame, uint64_t frame_number) {
                auto view = make_view_2d(frame);
                self.find_clusters(view, frame_number);
            },
            py::arg("frame"), py::arg("frame_number") = 0)

        .def(
            "find_clusters_batched",
            [](CF &self, py::array_t<uint16_t> frames, uint64_t first_frame) {
                // frames is expected as a 3D numpy array (n_frames, nrows, ncols)
                auto view = make_view_3d(frames);
                return self.find_clusters_batched(view, first_frame);
            },
            py::arg("frames"), py::arg("first_frame") = 0,
            R"(Process a 3D array of frames (n_frames, nrows, ncols) in parallel
across the configured CUDA streams. Returns a list of ClusterVector, one per
input frame.)");
}

} // namespace aare

#pragma GCC diagnostic pop