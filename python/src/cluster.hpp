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

template <typename Type, uint8_t ClusterSizeX, uint8_t ClusterSizeY,
          typename CoordType>
void define_cluster(py::module &m, const std::string &typestr) {
    auto class_name = fmt::format("Cluster{}", typestr);

    using ClusterType =
        Cluster<Type, ClusterSizeX, ClusterSizeY, CoordType, void>;
    py::class_<Cluster<Type, ClusterSizeX, ClusterSizeY, CoordType, void>>(
        m, class_name.c_str())

        .def(py::init([](uint8_t x, uint8_t y, py::array_t<Type> data) {
            py::buffer_info buf_info = data.request();
            Type *ptr = static_cast<Type *>(buf_info.ptr);
            Cluster<Type, ClusterSizeX, ClusterSizeY, CoordType, void> cluster;
            cluster.x = x;
            cluster.y = y;
            std::copy(ptr, ptr + ClusterSizeX * ClusterSizeY,
                      cluster.data); // Copy array contents
            return cluster;
        }))

        //.def(py::init<>())
        .def_readwrite("x", &ClusterType::x)
        .def_readwrite("y", &ClusterType::y)
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
}

template <typename ClusterType>
void define_cluster_vector(py::module &m, const std::string &typestr) {
    auto class_name = fmt::format("ClusterVector_{}", typestr);

    py::class_<ClusterVector<ClusterType>>(m, class_name.c_str(),
                                           py::buffer_protocol())

        .def(py::init()) // TODO change!!!
        /*
        .def("push_back",
             [](ClusterVector<ClusterType> &self, ClusterType &cl) {
                 //  auto view = make_view_2d(data);
                 self.push_back(cl);
             })
        */
        /*
        .def(
            "push_back",
            [](ClusterVector<ClusterType> &self, py::object obj) {
                ClusterType &cl = py::cast<ClusterType &>(obj);
                self.push_back(cl);
            },
            py::arg("cluster"))
        */

        .def("push_back",
             [](ClusterVector<ClusterType> &self, const ClusterType &cluster) {
                 self.push_back(cluster);
             })

        //.def("push_back", &ClusterVector<ClusterType>::push_back) //TODO
        // implement push_back
        .def_property_readonly("size", &ClusterVector<ClusterType>::size)
        .def("item_size", &ClusterVector<ClusterType>::item_size)
        .def_property_readonly("fmt",
                               [typestr]() { return fmt_format<ClusterType>; })
        /*
        .def("sum",
             [](ClusterVector<ClusterType> &self) {
                 auto *vec = new std::vector<T>(self.sum());
                 return return_vector(vec);
             })
        .def("sum_2x2",
             [](ClusterVector<ClusterType> &self) {
                 auto *vec = new std::vector<T>(self.sum_2x2());
                 return return_vector(vec);
             })
        */
        .def_property_readonly("cluster_size_x",
                               &ClusterVector<ClusterType>::cluster_size_x)
        .def_property_readonly("cluster_size_y",
                               &ClusterVector<ClusterType>::cluster_size_y)
        .def_property_readonly("capacity",
                               &ClusterVector<ClusterType>::capacity)
        .def_property("frame_number", &ClusterVector<ClusterType>::frame_number,
                      &ClusterVector<ClusterType>::set_frame_number)
        .def_buffer(
            [typestr](ClusterVector<ClusterType> &self) -> py::buffer_info {
                return py::buffer_info(
                    self.data(),             /* Pointer to buffer */
                    self.item_size(),        /* Size of one scalar */
                    fmt_format<ClusterType>, /* Format descriptor */
                    1,                       /* Number of dimensions */
                    {self.size()},           /* Buffer dimensions */
                    {self.item_size()} /* Strides (in bytes) for each index */
                );
            });
}

template <typename ClusterType>
void define_cluster_finder_mt_bindings(py::module &m,
                                       const std::string &typestr) {
    auto class_name = fmt::format("ClusterFinderMT_{}", typestr);

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

template <typename ClusterType>
void define_cluster_collector_bindings(py::module &m,
                                       const std::string &typestr) {
    auto class_name = fmt::format("ClusterCollector_{}", typestr);

    py::class_<ClusterCollector<ClusterType>>(m, class_name.c_str())
        .def(py::init<ClusterFinderMT<ClusterType, uint16_t, double> *>())
        .def("stop", &ClusterCollector<ClusterType>::stop)
        .def(
            "steal_clusters",
            [](ClusterCollector<ClusterType> &self) {
                auto v = new std::vector<ClusterVector<ClusterType>>(
                    self.steal_clusters());
                return v;
            },
            py::return_value_policy::take_ownership);
}

template <typename ClusterType>
void define_cluster_file_sink_bindings(py::module &m,
                                       const std::string &typestr) {
    auto class_name = fmt::format("ClusterFileSink_{}", typestr);

    py::class_<ClusterFileSink<ClusterType>>(m, class_name.c_str())
        .def(py::init<ClusterFinderMT<ClusterType, uint16_t, double> *,
                      const std::filesystem::path &>())
        .def("stop", &ClusterFileSink<ClusterType>::stop);
}

template <typename ClusterType>
void define_cluster_finder_bindings(py::module &m, const std::string &typestr) {
    auto class_name = fmt::format("ClusterFinder_{}", typestr);

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
                auto v = new ClusterVector<ClusterType>(
                    self.steal_clusters(realloc_same_capacity));
                return v;
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

    m.def("hitmap",
          [](std::array<size_t, 2> image_size, ClusterVector<ClusterType> &cv) {
              py::array_t<int32_t> hitmap(image_size);
              auto r = hitmap.mutable_unchecked<2>();

              // Initialize hitmap to 0
              for (py::ssize_t i = 0; i < r.shape(0); i++)
                  for (py::ssize_t j = 0; j < r.shape(1); j++)
                      r(i, j) = 0;

              size_t stride = cv.item_size();
              auto ptr = cv.data();
              for (size_t i = 0; i < cv.size(); i++) {
                  auto x = *reinterpret_cast<int16_t *>(ptr);
                  auto y = *reinterpret_cast<int16_t *>(ptr + sizeof(int16_t));
                  r(y, x) += 1;
                  ptr += stride;
              }
              return hitmap;
          });
}
