#include "aare/ClusterFile.hpp"
#include "aare/defs.hpp"

#include <cstdint>
#include <filesystem>
#include <pybind11/iostream.h>
#include <pybind11/numpy.h>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include <pybind11/stl/filesystem.h>
#include <string>

namespace py = pybind11;
using namespace ::aare;

void define_cluster_file_io_bindings(py::module &m) {
    PYBIND11_NUMPY_DTYPE(Cluster, x, y, data);

    py::class_<ClusterFile>(m, "ClusterFile")
        .def(py::init<const std::filesystem::path &, size_t,
                      const std::string &>(),
             py::arg(), py::arg("chunk_size") = 1000, py::arg("mode") = "r")
        .def("read_clusters",
             [](ClusterFile &self, size_t n_clusters) {
                 auto *vec =
                     new std::vector<Cluster>(self.read_clusters(n_clusters));
                 return return_vector(vec);
             })
        .def("read_frame",
             [](ClusterFile &self) {
                 int32_t frame_number;
                 auto *vec =
                     new std::vector<Cluster>(self.read_frame(frame_number));
                 return py::make_tuple(frame_number, return_vector(vec));
             })
        .def("write_frame", &ClusterFile::write_frame)
        .def("read_cluster_with_cut",
             [](ClusterFile &self, size_t n_clusters,
                py::array_t<double> noise_map, int nx, int ny) {
                 auto view = make_view_2d(noise_map);
                 auto *vec =
                     new std::vector<Cluster>(self.read_cluster_with_cut(
                         n_clusters, view.data(), nx, ny));
                 return return_vector(vec);
             })
        .def("__enter__", [](ClusterFile &self) { return &self; })
        .def("__exit__",
             [](ClusterFile &self,
                const std::optional<pybind11::type> &exc_type,
                const std::optional<pybind11::object> &exc_value,
                const std::optional<pybind11::object> &traceback) {
                 self.close();
             })
        .def("__iter__", [](ClusterFile &self) { return &self; })
        .def("__next__", [](ClusterFile &self) {
            auto vec =
                new std::vector<Cluster>(self.read_clusters(self.chunk_size()));
            if (vec->size() == 0) {
                throw py::stop_iteration();
            }
            return return_vector(vec);
        });
}