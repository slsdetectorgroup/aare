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

template <typename Type, uint8_t ClusterSizeX, uint8_t ClusterSizeY,
          typename CoordType>
void define_cluster(py::module &m, const std::string &typestr) {
    auto class_name = fmt::format("Cluster{}", typestr);

    py::class_<Cluster<Type, ClusterSizeX, ClusterSizeY, CoordType, void>>(
        m, class_name.c_str(), py::buffer_protocol())

        .def(py::init([](uint8_t x, uint8_t y, py::array_t<Type> data) {
            py::buffer_info buf_info = data.request();
            Type *ptr = static_cast<Type *>(buf_info.ptr);
            Cluster<Type, ClusterSizeX, ClusterSizeY, CoordType, void> cluster;
            cluster.x = x;
            cluster.y = y;
            std::copy(ptr, ptr + ClusterSizeX * ClusterSizeY,
                      cluster.data); // Copy array contents
            return cluster;
        }));

    /*
    .def_property(
        "data",
        [](ClusterType &c) -> py::array {
            return py::array(py::buffer_info(
                c.data, sizeof(Type),
                py::format_descriptor<Type>::format(), // Type
                                                       // format
                1, // Number of dimensions
                {static_cast<ssize_t>(ClusterSizeX *
                                      ClusterSizeY)}, // Shape (flattened)
                {sizeof(Type)} // Stride (step size between elements)
                ));
        },
        [](ClusterType &c, py::array_t<Type> arr) {
            py::buffer_info buf_info = arr.request();
            Type *ptr = static_cast<Type *>(buf_info.ptr);
            std::copy(ptr, ptr + ClusterSizeX * ClusterSizeY,
                      c.data); // TODO dont iterate over centers!!!

        });
    */
}




template <typename T, uint8_t ClusterSizeX, uint8_t ClusterSizeY,
          typename CoordType = uint16_t>
void define_cluster_finder_mt_bindings(py::module &m,
                                       const std::string &typestr) {
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

template <typename T, uint8_t ClusterSizeX, uint8_t ClusterSizeY,
          typename CoordType = uint16_t>
void define_cluster_collector_bindings(py::module &m,
                                       const std::string &typestr) {
    auto class_name = fmt::format("ClusterCollector_{}", typestr);

    using ClusterType = Cluster<T, ClusterSizeX, ClusterSizeY, CoordType>;

    py::class_<ClusterCollector<ClusterType>>(m, class_name.c_str())
        .def(py::init<ClusterFinderMT<ClusterType, uint16_t, double> *>())
        .def("stop", &ClusterCollector<ClusterType>::stop)
        .def(
            "steal_clusters",
            [](ClusterCollector<ClusterType> &self) {
                auto v = new std::vector<ClusterVector<ClusterType>>(
                    self.steal_clusters());
                return v; // TODO change!!!
            },
            py::return_value_policy::take_ownership);
}

template <typename T, uint8_t ClusterSizeX, uint8_t ClusterSizeY,
          typename CoordType = uint16_t>
void define_cluster_file_sink_bindings(py::module &m,
                                       const std::string &typestr) {
    auto class_name = fmt::format("ClusterFileSink_{}", typestr);

    using ClusterType = Cluster<T, ClusterSizeX, ClusterSizeY, CoordType>;

    py::class_<ClusterFileSink<ClusterType>>(m, class_name.c_str())
        .def(py::init<ClusterFinderMT<ClusterType, uint16_t, double> *,
                      const std::filesystem::path &>())
        .def("stop", &ClusterFileSink<ClusterType>::stop);
}

template <typename T, uint8_t ClusterSizeX, uint8_t ClusterSizeY,
          typename CoordType = uint16_t>
void define_cluster_finder_bindings(py::module &m, const std::string &typestr) {
    auto class_name = fmt::format("ClusterFinder_{}", typestr);

    using ClusterType = Cluster<T, ClusterSizeX, ClusterSizeY, CoordType>;

    py::class_<ClusterFinder<ClusterType, uint16_t, pd_type>>(
        m, class_name.c_str())
        .def(py::init<Shape<2>, pd_type, size_t>(), py::arg("image_size"),
             py::arg("n_sigma") = 5.0, py::arg("capacity") = 1'000'000)
        .def("push_pedestal_frame",
             [](ClusterFinder<ClusterType, uint16_t, pd_type> &self,
                py::array_t<uint16_t> frame) {
                 auto view = make_view_2d(frame);
                 self.push_pedestal_frame(view);
             })
        .def("clear_pedestal",
             &ClusterFinder<ClusterType, uint16_t, pd_type>::clear_pedestal)
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
            py::arg(), py::arg("frame_number") = 0);

}
#pragma GCC diagnostic pop
