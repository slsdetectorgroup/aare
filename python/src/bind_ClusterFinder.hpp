// SPDX-License-Identifier: MPL-2.0
#pragma once
#include "aare/ClusterCollector.hpp"
#include "aare/ClusterFileSink.hpp"
#include "aare/ClusterFinder.hpp"
#include "aare/ClusterFinderMT.hpp"
#include "aare/ClusterVector.hpp"
#include "aare/NDView.hpp"
#include "aare/Pedestal.hpp"
#include "aare/StateFile.hpp"

#include "module_config.hpp"
#include "np_helper.hpp"

#include <cstdint>
#include <filesystem>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include <pybind11/stl/filesystem.h>
#include <pybind11/stl_bind.h>

namespace py = pybind11;

using namespace aare;

template <typename T, uint8_t ClusterSizeX, uint8_t ClusterSizeY,
          typename CoordType = uint16_t>
void define_ClusterFinder(py::module &m, const std::string &typestr) {
    auto class_name = fmt::format("ClusterFinder_{}", typestr);

    using ClusterType = Cluster<T, ClusterSizeX, ClusterSizeY, CoordType>;

    py::class_<ClusterFinder<ClusterType, uint16_t, pd_type>>(
        m, class_name.c_str())
        .def(py::init<Shape<2>, pd_type, size_t, size_t>(),
             py::arg("image_size"), py::arg("n_sigma") = 5.0,
             py::arg("capacity") = 1'000'000,
             py::arg("min_pedestal_samples") = 1000)

        .def_property(
            "nSigma",
            &ClusterFinder<ClusterType, uint16_t, pd_type>::get_nSigma,
            &ClusterFinder<ClusterType, uint16_t, pd_type>::set_nSigma,
            R"(number of sigma above the pedestal to consider a photon during cluster finding.)")

        .def(
            "push_pedestal_frame",
            [](ClusterFinder<ClusterType, uint16_t, pd_type> &self,
               py::array_t<uint16_t> frame) {
                auto view = make_view_2d(frame);
                self.push_pedestal_frame(view);
            },
            py::arg("frame").noconvert())
        .def("clear_pedestal",
             &ClusterFinder<ClusterType, uint16_t, pd_type>::clear_pedestal)
        .def("update_threshold",
             &ClusterFinder<ClusterType, uint16_t, pd_type>::update_threshold)
        .def_property_readonly(
            "pedestal_ready",
            &ClusterFinder<ClusterType, uint16_t, pd_type>::pedestal_ready,
            R"(True once enough pedestal frames have been pushed to find clusters.)")
        .def_property_readonly(
            "pedestal",
            [](ClusterFinder<ClusterType, uint16_t, pd_type> &self) {
                auto pd = new NDArray<pd_type, 2>{};
                *pd = self.pedestal();
                return return_image_data(pd);
            })
        .def_property_readonly(
            "noise",
            [](ClusterFinder<ClusterType, uint16_t, pd_type> &self) {
                auto arr = new NDArray<pd_type, 2>{};
                *arr = self.noise();
                return return_image_data(arr);
            })
        .def(
            "steal_clusters",
            [](ClusterFinder<ClusterType, uint16_t, pd_type> &self,
               bool realloc_same_capacity) {
                ClusterVector<ClusterType> clusters =
                    self.steal_clusters(realloc_same_capacity);
                return clusters;
            },
            py::arg("realloc_same_capacity") = false)
        .def(
            "find_clusters",
            [](ClusterFinder<ClusterType, uint16_t, pd_type> &self,
               py::array_t<uint16_t> frame, uint64_t frame_number) {
                auto view = make_view_2d(frame);
                self.find_clusters(view, frame_number);
                return;
            },
            py::arg("frame").noconvert(), py::arg("frame_number") = 0)
        .def(
            "save_state",
            [](const ClusterFinder<ClusterType, uint16_t, pd_type> &self,
               const std::filesystem::path &filename) {
                aare::save_state(self.state(), filename);
            },
            py::arg("filename"),
            "Write the complete state of the cluster finder to a state file: "
            "the pedestal, whether it is still initializing or ready, nSigma "
            "and the threshold as last computed. An existing file is "
            "replaced. Clusters that have not been taken with "
            "steal_clusters() are not stored. A finder that loads the file "
            "with load_state() continues exactly as this one would.")
        // An instance method: as a static method that returns a new finder
        // it could be called on an existing one, which then stayed unchanged
        .def(
            "load_state",
            [](ClusterFinder<ClusterType, uint16_t, pd_type> &self,
               const std::filesystem::path &filename) {
                self.set_state(
                    aare::load_state<aare::ClusterFinderState>(filename));
            },
            py::arg("filename"),
            "Replace the state of this cluster finder with the one in a "
            "state file written by save_state(). nSigma and "
            "min_pedestal_samples are taken from the file; the cluster size "
            "and the image shape must be those of this finder, and the dtype "
            "stays that of this finder whatever the file records. Clusters "
            "that have not been taken with steal_clusters() are dropped. If "
            "the file cannot be loaded the finder is left unchanged.")
        .def_static(
            "from_state_file",
            [](const std::filesystem::path &filename, size_t capacity) {
                return ClusterFinder<ClusterType, uint16_t, pd_type>(
                    aare::load_state<aare::ClusterFinderState>(filename),
                    capacity);
            },
            py::arg("filename"), py::arg("capacity") = 1'000'000,
            "Create a cluster finder of this cluster size and dtype from a "
            "state file written by save_state(), whatever dtype the file "
            "records. The file must come from a finder with the same cluster "
            "size. The image shape, nSigma and min_pedestal_samples are "
            "taken from the file.");
}
