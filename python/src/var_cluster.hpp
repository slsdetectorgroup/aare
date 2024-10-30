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
    py::class_<VarClusterFinder<double>>(m, "VarClusterFinder")
        .def(py::init<Shape<2>, double>())
        .def("labeled",
             [](VarClusterFinder<double> &self) {
                 auto ptr = new NDArray<int, 2>(self.labeled());
                 return return_image_data(ptr);
             })
        .def("find_clusters",
             [](VarClusterFinder<double> &self,
                py::array_t<double, py::array::c_style | py::array::forcecast>
                    img) {
                 auto view = make_view_2d(img);
                 self.find_clusters(view);
             })
        .def("steal_hits",
             [](VarClusterFinder<double> &self) {
                 auto ptr = new std::vector<VarClusterFinder<double>::Hit>(
                     self.steal_hits());
                 return return_vector(ptr);
             })
        .def("total_clusters", &VarClusterFinder<double>::total_clusters);

}