#include "aare/ClusterFinder.hpp"
#include "aare/NDView.hpp"
#include "aare/Pedestal.hpp"
#include "np_helper.hpp"

#include <cstdint>
#include <filesystem>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

namespace py = pybind11;

void define_cluster_finder_bindings(py::module &m) {
    py::class_<ClusterFinder<uint16_t, double>>(m, "ClusterFinder")
        .def(py::init<Shape<2>, Shape<2>>())
        .def("push_pedestal_frame",
             [](ClusterFinder<uint16_t, double> &self,
                py::array_t<uint16_t> frame) {
                 auto view = make_view_2d(frame);
                 self.push_pedestal_frame(view);
             })
        .def("pedestal",
             [](ClusterFinder<uint16_t, double> &self) {
                 auto m = new NDArray<double, 2>{};
                 *m = self.pedestal();
                 return return_image_data(m);
             })
        .def("find_clusters_without_threshold",
             [](ClusterFinder<uint16_t, double> &self,
                py::array_t<uint16_t> frame) {
                 auto view = make_view_2d(frame);
                 auto clusters = self.find_clusters_without_threshold(view);
                 return clusters;
             });

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