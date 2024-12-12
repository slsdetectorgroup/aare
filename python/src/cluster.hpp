#include "aare/ClusterFinder.hpp"
#include "aare/ClusterVector.hpp"
#include "aare/NDView.hpp"
#include "aare/Pedestal.hpp"
#include "np_helper.hpp"

#include <cstdint>
#include <filesystem>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

namespace py = pybind11;
using pd_type = float;

template <typename T>
void define_cluster_vector(py::module &m, const std::string &typestr) {
    auto class_name = fmt::format("ClusterVector_{}", typestr);
    py::class_<ClusterVector<T>>(m, class_name.c_str(), py::buffer_protocol())
        .def(py::init<int, int>())
        .def_property_readonly("size", &ClusterVector<T>::size)
        .def("element_offset",
             py::overload_cast<>(&ClusterVector<T>::element_offset, py::const_))
        .def_property_readonly("fmt",
             [typestr](ClusterVector<T> &self) {
                 return fmt::format(
                     self.fmt_base(), self.cluster_size_x(),
                     self.cluster_size_y(), typestr);
             })
        .def("sum", [](ClusterVector<T> &self) {
            auto *vec = new std::vector<T>(self.sum());
            return return_vector(vec);
        })
        .def_buffer([typestr](ClusterVector<T> &self) -> py::buffer_info {
            return py::buffer_info(
                self.data(),           /* Pointer to buffer */
                self.element_offset(), /* Size of one scalar */
                fmt::format(self.fmt_base(), self.cluster_size_x(),
                            self.cluster_size_y(),
                            typestr), /* Format descriptor */
                1,                    /* Number of dimensions */
                {self.size()},           /* Buffer dimensions */
                {self.element_offset()}  /* Strides (in bytes) for each index */
            );
        });
}

void define_cluster_finder_bindings(py::module &m) {
    py::class_<ClusterFinder<uint16_t, pd_type>>(m, "ClusterFinder")
        .def(py::init<Shape<2>, Shape<2>, pd_type, size_t>(), py::arg("image_size"),
             py::arg("cluster_size"), py::arg("n_sigma") = 5.0,
             py::arg("capacity") = 1'000'000)
        .def("push_pedestal_frame",
             [](ClusterFinder<uint16_t, pd_type> &self,
                py::array_t<uint16_t> frame) {
                 auto view = make_view_2d(frame);
                 self.push_pedestal_frame(view);
             })
        .def("pedestal",
             [](ClusterFinder<uint16_t, pd_type> &self) {
                 auto pd = new NDArray<pd_type, 2>{};
                 *pd = self.pedestal();
                 return return_image_data(pd);
             })
        .def("noise",
             [](ClusterFinder<uint16_t, pd_type> &self) {
                 auto arr = new NDArray<pd_type, 2>{};
                 *arr = self.noise();
                 return return_image_data(arr);
             })
        .def("steal_clusters",
             [](ClusterFinder<uint16_t, pd_type> &self, bool realloc_same_capacity) {
                 auto v = new ClusterVector<int>(self.steal_clusters(realloc_same_capacity));
                 return v;
             }, py::arg("realloc_same_capacity") = false)
        .def("find_clusters",
             [](ClusterFinder<uint16_t, pd_type> &self,
                py::array_t<uint16_t> frame) {
                 auto view = make_view_2d(frame);
                 self.find_clusters(view);
                 return;
             });

    m.def("hitmap", [](std::array<size_t, 2> image_size, ClusterVector<int32_t>& cv){
        
        py::array_t<int32_t> hitmap(image_size);
        auto r = hitmap.mutable_unchecked<2>(); 

        // Initialize hitmap to 0
        for (py::ssize_t i = 0; i < r.shape(0); i++)
            for (py::ssize_t j = 0; j < r.shape(1); j++)
                r(i, j) = 0;

        size_t stride = cv.element_offset();
        auto ptr = cv.data();
        for(size_t i=0; i<cv.size(); i++){
            auto x = *reinterpret_cast<int16_t*>(ptr);
            auto y = *reinterpret_cast<int16_t*>(ptr+sizeof(int16_t));
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