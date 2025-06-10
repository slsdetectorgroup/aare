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

        .def(py::init([](uint8_t x, uint8_t y, py::array_t<Type> data) {
            py::buffer_info buf_info = data.request();
            Cluster<Type, ClusterSizeX, ClusterSizeY, CoordType> cluster;
            cluster.x = x;
            cluster.y = y;
            auto r = data.template unchecked<1>(); // no bounds checks
            for (py::ssize_t i = 0; i < data.size(); ++i) {
                cluster.data[i] = r(i);
            }
            return cluster;
        }));

    /*
    //TODO! Review if to keep or not
    .def_property(
        "data",
        [](ClusterType &c) -> py::array {
            return py::array(py::buffer_info(
                c.data, sizeof(Type),
                py::format_descriptor<Type>::format(), // Type
                                                       // format
                1, // Number of dimensions
                {static_cast<ssize_t>(ClusterSizeX *
                                      ClusterSizeY)}, // Shape (flattened)
                {sizeof(Type)} // Stride (step size between elements)
                ));
        },
        [](ClusterType &c, py::array_t<Type> arr) {
            py::buffer_info buf_info = arr.request();
            Type *ptr = static_cast<Type *>(buf_info.ptr);
            std::copy(ptr, ptr + ClusterSizeX * ClusterSizeY,
                      c.data); // TODO dont iterate over centers!!!

        });
    */
}

#pragma GCC diagnostic pop