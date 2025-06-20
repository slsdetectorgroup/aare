#include "aare/CalculateEta.hpp"
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

// Disable warnings for unused parameters, as we ignore some
// in the __exit__ method
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"

namespace py = pybind11;
using namespace ::aare;

template <typename Type, uint8_t CoordSizeX, uint8_t CoordSizeY,
          typename CoordType = uint16_t>
void define_ClusterFile(py::module &m, const std::string &typestr) {

    using ClusterType = Cluster<Type, CoordSizeX, CoordSizeY, CoordType>;

    auto class_name = fmt::format("ClusterFile_{}", typestr);

    py::class_<ClusterFile<ClusterType>>(m, class_name.c_str())
        .def(py::init<const std::filesystem::path &, size_t,
                      const std::string &>(),
             py::arg(), py::arg("chunk_size") = 1000, py::arg("mode") = "r")
        .def(
            "read_clusters",
            [](ClusterFile<ClusterType> &self, size_t n_clusters) {
                auto v = new ClusterVector<ClusterType>(
                    self.read_clusters(n_clusters));
                return v;
            },
            py::return_value_policy::take_ownership, py::arg("n_clusters"))
        .def("read_frame",
             [](ClusterFile<ClusterType> &self) {
                 auto v = new ClusterVector<ClusterType>(self.read_frame());
                 return v;
             })
        .def("set_roi", &ClusterFile<ClusterType>::set_roi,
             py::arg("roi"))
        .def(
            "set_noise_map",
            [](ClusterFile<ClusterType> &self, py::array_t<int32_t> noise_map) {
                auto view = make_view_2d(noise_map);
                self.set_noise_map(view);
            }, py::arg("noise_map"))

        .def("set_gain_map",
             [](ClusterFile<ClusterType> &self, py::array_t<double> gain_map) {
                 auto view = make_view_2d(gain_map);
                 self.set_gain_map(view);
             })

        .def("close", &ClusterFile<ClusterType>::close)
        .def("write_frame", &ClusterFile<ClusterType>::write_frame)
        .def("__enter__", [](ClusterFile<ClusterType> &self) { return &self; })
        .def("__exit__",
             [](ClusterFile<ClusterType> &self,
                const std::optional<pybind11::type> &exc_type,
                const std::optional<pybind11::object> &exc_value,
                const std::optional<pybind11::object> &traceback) {
                 self.close();
             })
        .def("__iter__", [](ClusterFile<ClusterType> &self) { return &self; })
        .def("__next__", [](ClusterFile<ClusterType> &self) {
            auto v = new ClusterVector<ClusterType>(
                self.read_clusters(self.chunk_size()));
            if (v->size() == 0) {
                throw py::stop_iteration();
            }
            return v;
        });
}

template <typename Type, uint8_t CoordSizeX, uint8_t CoordSizeY,
          typename CoordType = uint16_t>
void register_calculate_eta(py::module &m) {
    using ClusterType = Cluster<Type, CoordSizeX, CoordSizeY, CoordType>;
    m.def("calculate_eta2",
          [](const aare::ClusterVector<ClusterType> &clusters) {
              auto eta2 = new NDArray<double, 2>(calculate_eta2(clusters));
              return return_image_data(eta2);
          });
}

#pragma GCC diagnostic pop