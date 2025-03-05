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

template <typename T>
void define_cluster_vector(py::module &m, const std::string &typestr) {
    auto class_name = fmt::format("ClusterVector_{}", typestr);
    py::class_<ClusterVector<T>>(m, class_name.c_str(), py::buffer_protocol())
        .def(py::init<int, int>(),
    py::arg("cluster_size_x") = 3, py::arg("cluster_size_y") = 3)
        .def("push_back",
             [](ClusterVector<T> &self, int x, int y, py::array_t<T> data) {
                //  auto view = make_view_2d(data);
                 self.push_back(x, y, reinterpret_cast<const std::byte*>(data.data()));
             })
        .def_property_readonly("size", &ClusterVector<T>::size)
        .def("item_size", &ClusterVector<T>::item_size)
        .def_property_readonly("fmt",
                               [typestr](ClusterVector<T> &self) {
                                   return fmt::format(
                                       self.fmt_base(), self.cluster_size_x(),
                                       self.cluster_size_y(), typestr);
                               })
        .def("sum",
             [](ClusterVector<T> &self) {
                 auto *vec = new std::vector<T>(self.sum());
                 return return_vector(vec);
             })
        .def("sum_2x2", [](ClusterVector<T> &self) {
            auto *vec = new std::vector<T>(self.sum_2x2());
            return return_vector(vec);
        })
        .def_property_readonly("cluster_size_x", &ClusterVector<T>::cluster_size_x)
        .def_property_readonly("cluster_size_y", &ClusterVector<T>::cluster_size_y)
        .def_property_readonly("capacity", &ClusterVector<T>::capacity)
        .def_property("frame_number", &ClusterVector<T>::frame_number,
                      &ClusterVector<T>::set_frame_number)
        .def_buffer([typestr](ClusterVector<T> &self) -> py::buffer_info {
            return py::buffer_info(
                self.data(),      /* Pointer to buffer */
                self.item_size(), /* Size of one scalar */
                fmt::format(self.fmt_base(), self.cluster_size_x(),
                            self.cluster_size_y(),
                            typestr), /* Format descriptor */
                1,                    /* Number of dimensions */
                {self.size()},        /* Buffer dimensions */
                {self.item_size()}    /* Strides (in bytes) for each index */
            );
        });
}

void define_cluster_finder_mt_bindings(py::module &m) {
    py::class_<ClusterFinderMT<uint16_t, pd_type>>(m, "ClusterFinderMT")
        .def(py::init<Shape<2>, Shape<2>, pd_type, size_t, size_t>(),
             py::arg("image_size"), py::arg("cluster_size"),
             py::arg("n_sigma") = 5.0, py::arg("capacity") = 2048,
             py::arg("n_threads") = 3)
        .def("push_pedestal_frame",
             [](ClusterFinderMT<uint16_t, pd_type> &self,
                py::array_t<uint16_t> frame) {
                 auto view = make_view_2d(frame);
                 self.push_pedestal_frame(view);
             })
        .def(
            "find_clusters",
            [](ClusterFinderMT<uint16_t, pd_type> &self,
               py::array_t<uint16_t> frame, uint64_t frame_number) {
                auto view = make_view_2d(frame);
                self.find_clusters(view, frame_number);
                return;
            },
            py::arg(), py::arg("frame_number") = 0)
        .def("clear_pedestal", &ClusterFinderMT<uint16_t, pd_type>::clear_pedestal)
        .def("sync", &ClusterFinderMT<uint16_t, pd_type>::sync)
        .def("stop", &ClusterFinderMT<uint16_t, pd_type>::stop)
        .def("start", &ClusterFinderMT<uint16_t, pd_type>::start)
        .def("pedestal",
             [](ClusterFinderMT<uint16_t, pd_type> &self, size_t thread_index) {
                 auto pd = new NDArray<pd_type, 2>{};
                 *pd = self.pedestal(thread_index);
                 return return_image_data(pd);
             },py::arg("thread_index") = 0)
        .def("noise",
             [](ClusterFinderMT<uint16_t, pd_type> &self, size_t thread_index) {
                 auto arr = new NDArray<pd_type, 2>{};
                 *arr = self.noise(thread_index);
                 return return_image_data(arr);
             },py::arg("thread_index") = 0);
}

void define_cluster_collector_bindings(py::module &m) {
    py::class_<ClusterCollector>(m, "ClusterCollector")
        .def(py::init<ClusterFinderMT<uint16_t, double, int32_t> *>())
        .def("stop", &ClusterCollector::stop)
        .def(
            "steal_clusters",
            [](ClusterCollector &self) {
                auto v =
                    new std::vector<ClusterVector<int>>(self.steal_clusters());
                return v;
            },
            py::return_value_policy::take_ownership);
}

void define_cluster_file_sink_bindings(py::module &m) {
    py::class_<ClusterFileSink>(m, "ClusterFileSink")
        .def(py::init<ClusterFinderMT<uint16_t, double, int32_t> *,
                      const std::filesystem::path &>())
        .def("stop", &ClusterFileSink::stop);
}

void define_cluster_finder_bindings(py::module &m) {
    py::class_<ClusterFinder<uint16_t, pd_type>>(m, "ClusterFinder")
        .def(py::init<Shape<2>, Shape<2>, pd_type, size_t>(),
             py::arg("image_size"), py::arg("cluster_size"),
             py::arg("n_sigma") = 5.0, py::arg("capacity") = 1'000'000)
        .def("push_pedestal_frame",
             [](ClusterFinder<uint16_t, pd_type> &self,
                py::array_t<uint16_t> frame) {
                 auto view = make_view_2d(frame);
                 self.push_pedestal_frame(view);
             })
        .def("clear_pedestal", &ClusterFinder<uint16_t, pd_type>::clear_pedestal)
        .def_property_readonly("pedestal",
                               [](ClusterFinder<uint16_t, pd_type> &self) {
                                   auto pd = new NDArray<pd_type, 2>{};
                                   *pd = self.pedestal();
                                   return return_image_data(pd);
                               })
        .def_property_readonly("noise",
                               [](ClusterFinder<uint16_t, pd_type> &self) {
                                   auto arr = new NDArray<pd_type, 2>{};
                                   *arr = self.noise();
                                   return return_image_data(arr);
                               })
        .def(
            "steal_clusters",
            [](ClusterFinder<uint16_t, pd_type> &self,
               bool realloc_same_capacity) {
                auto v = new ClusterVector<int>(
                    self.steal_clusters(realloc_same_capacity));
                return v;
            },
            py::arg("realloc_same_capacity") = false)
        .def(
            "find_clusters",
            [](ClusterFinder<uint16_t, pd_type> &self,
               py::array_t<uint16_t> frame, uint64_t frame_number) {
                auto view = make_view_2d(frame);
                self.find_clusters(view, frame_number);
                return;
            },
            py::arg(), py::arg("frame_number") = 0);

    m.def("hitmap",
          [](std::array<size_t, 2> image_size, ClusterVector<int32_t> &cv) {
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
    define_cluster_vector<int>(m, "i");
    define_cluster_vector<double>(m, "d");
    define_cluster_vector<float>(m, "f");

    py::class_<DynamicCluster>(m, "DynamicCluster", py::buffer_protocol())
        .def(py::init<int, int, Dtype>())
        .def("size", &DynamicCluster::size)
        .def("begin", &DynamicCluster::begin)
        .def("end", &DynamicCluster::end)
        .def_readwrite("x", &DynamicCluster::x)
        .def_readwrite("y", &DynamicCluster::y)
        .def_buffer([](DynamicCluster &c) -> py::buffer_info {
            return py::buffer_info(c.data(), c.dt.bytes(), c.dt.format_descr(),
                                   1, {c.size()}, {c.dt.bytes()});
        })

        .def("__repr__", [](const DynamicCluster &a) {
            return "<DynamicCluster: x: " + std::to_string(a.x) +
                   ", y: " + std::to_string(a.y) + ">";
        });
}