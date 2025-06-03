#include "aare/VarClusterFinder.hpp"
#include "np_helper.hpp"
// #include "aare/defs.hpp"
// #include "aare/fClusterFileV2.hpp"

#include <cstdint>
// #include <filesystem>
#include <pybind11/numpy.h>
// #include <pybind11/iostream.h>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
// #include <pybind11/stl/filesystem.h>
// #include <string>


namespace py = pybind11;
using namespace::aare;


void define_var_cluster_finder_bindings(py::module &m) {
    PYBIND11_NUMPY_DTYPE(VarClusterFinder<double>::Hit, size, row, col,
                         reserved, energy, max, rows, cols, enes);

    py::class_<VarClusterFinder<double>>(m, "VarClusterFinder")
        .def(py::init<Shape<2>, double>())
        .def("labeled",
             [](VarClusterFinder<double> &self) {
                 auto *ptr = new NDArray<int, 2>(self.labeled());
                 return return_image_data(ptr);
             })
        .def("set_noiseMap",
            [](VarClusterFinder<double> &self,
                py::array_t<double, py::array::c_style | py::array::forcecast>
                    noise_map) {
                auto noise_map_span = make_view_2d(noise_map);
                self.set_noiseMap(noise_map_span);
            })
        .def("set_peripheralThresholdFactor",
             &VarClusterFinder<double>::set_peripheralThresholdFactor)
        .def("find_clusters",
             [](VarClusterFinder<double> &self,
                py::array_t<double, py::array::c_style | py::array::forcecast>
                    img) {
                 auto view = make_view_2d(img);
                 self.find_clusters(view);
             })
        .def("find_clusters_X",
             [](VarClusterFinder<double> &self,
                py::array_t<double, py::array::c_style | py::array::forcecast>
                    img) {
                 auto img_span = make_view_2d(img);
                 self.find_clusters_X(img_span);
             })
        .def("single_pass",
             [](VarClusterFinder<double> &self,
                py::array_t<double, py::array::c_style | py::array::forcecast>
                    img) {
                 auto img_span = make_view_2d(img);
                 self.single_pass(img_span);
             })
        .def("hits",
             [](VarClusterFinder<double> &self) {
                 auto ptr = new std::vector<VarClusterFinder<double>::Hit>(
                     self.steal_hits());
                 return return_vector(ptr);
             })
        .def("clear_hits",
             [](VarClusterFinder<double> &self) {
                 self.clear_hits();
             })
        .def("steal_hits",
             [](VarClusterFinder<double> &self) {
                 auto ptr = new std::vector<VarClusterFinder<double>::Hit>(
                     self.steal_hits());
                 return return_vector(ptr);
             })
        .def("total_clusters", &VarClusterFinder<double>::total_clusters);

}