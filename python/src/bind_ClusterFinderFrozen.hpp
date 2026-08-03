// SPDX-License-Identifier: MPL-2.0
#include "aare/ClusterFinderFrozen.hpp"
#include "aare/ClusterVector.hpp"
#include "aare/NDView.hpp"
#include "aare/Pedestal.hpp"
#include "np_helper.hpp"

#include <cstdint>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include <pybind11/stl_bind.h>

namespace py = pybind11;
using pd_type = double;

using namespace aare;

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"

template <typename T, uint8_t ClusterSizeX, uint8_t ClusterSizeY,
          typename CoordType = uint16_t>
void define_ClusterFinderFrozen(py::module &m, const std::string &typestr) {
    auto class_name = fmt::format("ClusterFinderFrozen_{}", typestr);

    using ClusterType = Cluster<T, ClusterSizeX, ClusterSizeY, CoordType>;

    py::class_<ClusterFinderFrozen<ClusterType, uint16_t, pd_type>>(
        m, class_name.c_str())
        .def(py::init<Shape<2>, pd_type, size_t>(), py::arg("image_size"),
             py::arg("n_sigma") = 5.0, py::arg("capacity") = 1'000'000)

        .def_property(
            "nSigma",
            &ClusterFinderFrozen<ClusterType, uint16_t, pd_type>::get_nSigma,
            &ClusterFinderFrozen<ClusterType, uint16_t, pd_type>::set_nSigma,
            R"(number of sigma above the pedestal to consider a photon during cluster finding.)")

        .def("push_pedestal_frame",
             [](ClusterFinderFrozen<ClusterType, uint16_t, pd_type> &self,
                py::array_t<uint16_t> frame) {
                 auto view = make_view_2d(frame);
                 self.push_pedestal_frame(view);
             })
        .def("clear_pedestal", &ClusterFinderFrozen<ClusterType, uint16_t,
                                                    pd_type>::clear_pedestal)
        .def_property_readonly(
            "pedestal",
            [](ClusterFinderFrozen<ClusterType, uint16_t, pd_type> &self) {
                auto pd = new NDArray<pd_type, 2>{};
                *pd = self.pedestal();
                return return_image_data(pd);
            })
        .def_property_readonly(
            "noise",
            [](ClusterFinderFrozen<ClusterType, uint16_t, pd_type> &self) {
                auto arr = new NDArray<pd_type, 2>{};
                *arr = self.noise();
                return return_image_data(arr);
            })
        .def(
            "steal_clusters",
            [](ClusterFinderFrozen<ClusterType, uint16_t, pd_type> &self,
               bool realloc_same_capacity) {
                ClusterVector<ClusterType> clusters =
                    self.steal_clusters(realloc_same_capacity);
                return clusters;
            },
            py::arg("realloc_same_capacity") = false)
        .def(
            "find_clusters",
            [](ClusterFinderFrozen<ClusterType, uint16_t, pd_type> &self,
               py::array_t<uint16_t> frame, uint64_t frame_number) {
                auto view = make_view_2d(frame);
                self.find_clusters(view, frame_number);
                return;
            },
            py::arg(), py::arg("frame_number") = 0);
}

#pragma GCC diagnostic pop
