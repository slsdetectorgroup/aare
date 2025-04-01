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

//Disable warnings for unused parameters, as we ignore some
//in the __exit__ method
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"


namespace py = pybind11;
using namespace ::aare;

void define_cluster_file_io_bindings(py::module &m) {
    PYBIND11_NUMPY_DTYPE(Cluster3x3, x, y, data);

    py::class_<ClusterFile>(m, "ClusterFile")
        .def(py::init<const std::filesystem::path &, size_t,
                      const std::string &>(),
             py::arg(), py::arg("chunk_size") = 1000, py::arg("mode") = "r")
        .def("read_clusters",
             [](ClusterFile &self, size_t n_clusters) {
                auto v = new ClusterVector<int32_t>(self.read_clusters(n_clusters));
                return v;
             },py::return_value_policy::take_ownership)
        .def("read_frame",
             [](ClusterFile &self) {
                auto v = new ClusterVector<int32_t>(self.read_frame());
                return v;
             })
        .def("set_roi", &ClusterFile::set_roi)
        .def("set_noise_map", [](ClusterFile &self, py::array_t<int32_t> noise_map) {
            auto view = make_view_2d(noise_map);
            self.set_noise_map(view);
        })
        .def("set_gain_map", [](ClusterFile &self, py::array_t<double> gain_map) {
            auto view = make_view_2d(gain_map);
            self.set_gain_map(view);
        })
        .def("close", &ClusterFile::close)
        .def("write_frame", &ClusterFile::write_frame)
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
            auto v = new ClusterVector<int32_t>(self.read_clusters(self.chunk_size()));
            if (v->size() == 0) {
                throw py::stop_iteration();
            }
            return v;
        });

        m.def("calculate_eta2", []( aare::ClusterVector<int32_t> &clusters) {
            auto eta2 = new NDArray<double, 2>(calculate_eta2(clusters));
            return return_image_data(eta2);
        });
}

#pragma GCC diagnostic pop