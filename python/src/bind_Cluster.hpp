#include "aare/Cluster.hpp"

#include <cstdint>
#include <filesystem>
#include <fmt/format.h>
#include <pybind11/numpy.h>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include <pybind11/stl_bind.h>

namespace py = pybind11;
using pd_type = double;

using namespace aare;

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"

template <typename Type, uint8_t ClusterSizeX, uint8_t ClusterSizeY,
          typename CoordType>
void define_Cluster(py::module &m, const std::string &typestr) {
    auto class_name = fmt::format("Cluster{}", typestr);

    py::class_<Cluster<Type, ClusterSizeX, ClusterSizeY, CoordType>>(
        m, class_name.c_str(), py::buffer_protocol())

        .def(py::init([](uint8_t x, uint8_t y,
                         py::array_t<Type, py::array::forcecast> data) {
            py::buffer_info buf_info = data.request();
            Cluster<Type, ClusterSizeX, ClusterSizeY, CoordType> cluster;
            cluster.x = x;
            cluster.y = y;
            auto r = data.template unchecked<1>(); // no bounds checks
            for (py::ssize_t i = 0; i < data.size(); ++i) {
                cluster.data[i] = r(i);
            }
            return cluster;
        }))

        // TODO! Review if to keep or not
        .def_property_readonly(
            "data",
            [](Cluster<Type, ClusterSizeX, ClusterSizeY, CoordType> &c)
                -> py::array {
                return py::array(py::buffer_info(
                    c.data.data(), sizeof(Type),
                    py::format_descriptor<Type>::format(), // Type
                                                           // format
                    2, // Number of dimensions
                    {static_cast<ssize_t>(ClusterSizeX),
                     static_cast<ssize_t>(ClusterSizeY)}, // Shape (flattened)
                    {sizeof(Type) * ClusterSizeY, sizeof(Type)}
                    // Stride (step size between elements)
                    ));
            })

        .def_readonly("x",
                      &Cluster<Type, ClusterSizeX, ClusterSizeY, CoordType>::x)

        .def_readonly("y",
                      &Cluster<Type, ClusterSizeX, ClusterSizeY, CoordType>::y);
}

template <typename T, uint8_t ClusterSizeX, uint8_t ClusterSizeY,
          typename CoordType = int16_t>
void reduce_to_3x3(py::module &m) {

    m.def(
        "reduce_to_3x3",
        [](const Cluster<T, ClusterSizeX, ClusterSizeY, CoordType> &cl) {
            return reduce_to_3x3(cl);
        },
        py::return_value_policy::move,
        "Reduce cluster to 3x3 subcluster by taking the 3x3 subcluster with "
        "the highest photon energy.");
}

template <typename T, uint8_t ClusterSizeX, uint8_t ClusterSizeY,
          typename CoordType = int16_t>
void reduce_to_2x2(py::module &m) {

    m.def(
        "reduce_to_2x2",
        [](const Cluster<T, ClusterSizeX, ClusterSizeY, CoordType> &cl) {
            return reduce_to_2x2(cl);
        },
        py::return_value_policy::move,
        "Reduce cluster to 2x2 subcluster by taking the 2x2 subcluster with "
        "the highest photon energy.");
}

#pragma GCC diagnostic pop