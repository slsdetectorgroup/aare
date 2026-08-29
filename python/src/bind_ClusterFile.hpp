// SPDX-License-Identifier: MPL-2.0
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

    py::class_<ClusterFile<ClusterType>>(
        m, class_name.c_str(),
        "Read and write legacy binary cluster files. The format contains no "
        "cluster type or shape metadata, so this class must match the file.")
        .def(py::init<const std::filesystem::path &, size_t,
                      const std::string &>(),
             py::arg("fname"), py::arg("chunk_size") = 1000,
             py::arg("mode") = "r",
             "Open a cluster file. Mode must be 'r' to read, 'w' to truncate "
             "and write, or 'a' to append.")
        .def(
            "read_clusters",
            [](ClusterFile<ClusterType> &self, size_t n_clusters) {
                auto v = new ClusterVector<ClusterType>(
                    self.read_clusters(n_clusters));
                return v;
            },
            py::return_value_policy::take_ownership, py::arg("n_clusters"),
            "Read up to n_clusters without preserving frame boundaries. The "
            "result may combine frames, so its frame number is not reliable "
            "per-cluster metadata.")
        .def(
            "read_frame",
            [](ClusterFile<ClusterType> &self) {
                auto v = new ClusterVector<ClusterType>(self.read_frame());
                return v;
            },
            "Read and return the next complete frame with its frame number.")
        .def("set_roi", &ClusterFile<ClusterType>::set_roi, py::arg("roi"),
             "Select clusters whose centers lie within the half-open ROI.")
        .def("tell", &ClusterFile<ClusterType>::tell,
             "Return the current byte position in the file.")
        .def("estimate_n_clusters",
             &ClusterFile<ClusterType>::estimate_n_clusters,
             "Estimate the number of clusters from the file size. Frame "
             "headers can make this larger than the actual count.")
        .def(
            "set_noise_map",
            [](ClusterFile<ClusterType> &self, py::array_t<int32_t> noise_map) {
                auto view = make_view_2d(noise_map);
                self.set_noise_map(view);
            },
            py::arg("noise_map"),
            "Set a two-dimensional, C-contiguous int32 noise map indexed as "
            "[y, x]. The map must cover every cluster center coordinate.")

        .def(
            "set_gain_map",
            [](ClusterFile<ClusterType> &self, py::array_t<double> gain_map) {
                auto view = make_view_2d(gain_map);
                self.set_gain_map(view);
            },
            py::arg("gain_map"),
            "Set a two-dimensional, C-contiguous float64 gain map in "
            "ADU/energy, indexed as [y, x]. Clusters whose complete footprint "
            "extends beyond the map are retained with all data values set to "
            "zero.")

        .def("close", &ClusterFile<ClusterType>::close,
             "Close the file. Calling close more than once is safe.")
        .def("write_frame", &ClusterFile<ClusterType>::write_frame,
             py::arg("clusters"),
             "Write one ClusterVector, including its frame number.")
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

#pragma GCC diagnostic pop
