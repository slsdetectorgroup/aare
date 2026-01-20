#include "aare/ClusterCollector.hpp"
#include "aare/ClusterFileSink.hpp"
#include "aare/ClusterFinder.hpp"
#include "aare/ClusterFinderMT.hpp"
#include "aare/ClusterVector.hpp"
#include "aare/NDView.hpp"
#include "aare/Pedestal.hpp"
#include "np_helper.hpp"

#include <cstdint>
#include <filesystem>
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
void define_ClusterFinderMT(py::module &m, const std::string &typestr) {
    auto class_name = fmt::format("ClusterFinderMT_{}", typestr);

    using ClusterType = Cluster<T, ClusterSizeX, ClusterSizeY, CoordType>;

    py::class_<ClusterFinderMT<ClusterType, uint16_t, pd_type>>(
        m, class_name.c_str())
        .def(py::init<Shape<2>, pd_type, size_t, size_t>(),
             py::arg("image_size"), py::arg("n_sigma") = 5.0,
             py::arg("capacity") = 2048, py::arg("n_threads") = 3)
        .def("push_pedestal_frame",
             [](ClusterFinderMT<ClusterType, uint16_t, pd_type> &self,
                py::array_t<uint16_t> frame) {
                 auto view = make_view_2d(frame);
                 self.push_pedestal_frame(view);
             })
        .def(
            "find_clusters",
            [](ClusterFinderMT<ClusterType, uint16_t, pd_type> &self,
               py::array_t<uint16_t> frame, uint64_t frame_number) {
                auto view = make_view_2d(frame);
                self.find_clusters(view, frame_number);
                return;
            },
            py::arg(), py::arg("frame_number") = 0)
        .def_property_readonly(
            "cluster_size",
            [](ClusterFinderMT<ClusterType, uint16_t, pd_type> &self) {
                return py::make_tuple(ClusterSizeX, ClusterSizeY);
            })
        .def("clear_pedestal",
             &ClusterFinderMT<ClusterType, uint16_t, pd_type>::clear_pedestal)
        .def("sync", &ClusterFinderMT<ClusterType, uint16_t, pd_type>::sync)
        .def("stop", &ClusterFinderMT<ClusterType, uint16_t, pd_type>::stop)
        .def("start", &ClusterFinderMT<ClusterType, uint16_t, pd_type>::start)
        .def(
            "pedestal",
            [](ClusterFinderMT<ClusterType, uint16_t, pd_type> &self,
               size_t thread_index) {
                auto pd = new NDArray<pd_type, 2>{};
                *pd = self.pedestal(thread_index);
                return return_image_data(pd);
            },
            py::arg("thread_index") = 0)
        .def(
            "noise",
            [](ClusterFinderMT<ClusterType, uint16_t, pd_type> &self,
               size_t thread_index) {
                auto arr = new NDArray<pd_type, 2>{};
                *arr = self.noise(thread_index);
                return return_image_data(arr);
            },
            py::arg("thread_index") = 0);
}

#pragma GCC diagnostic pop