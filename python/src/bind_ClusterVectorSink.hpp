// SPDX-License-Identifier: MPL-2.0
#pragma once
#include "aare/ClusterVectorSink.hpp"

#include "bind_ClusterSinkBase_methods.hpp"

#include <cstdint>
#include <fmt/format.h>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include <string>

namespace py = pybind11;

template <typename T, uint8_t ClusterSizeX, uint8_t ClusterSizeY,
          typename CoordType = uint16_t>
void define_ClusterVectorSink(py::module &m, const std::string &typestr) {
    auto class_name = fmt::format("ClusterVectorSink_{}", typestr);

    using ClusterType = aare::Cluster<T, ClusterSizeX, ClusterSizeY, CoordType>;
    using Sink = aare::ClusterVectorSink<ClusterType>;

    py::class_<Sink> cls(m, class_name.c_str(),
                         R"(ClusterSink that keeps the clusters in memory.
Start it with collect(). steal_clusters() can be called while it is
running; call sync() on the finder first to get the clusters of every frame
given to it so far.)");
    cls.def(py::init<>());
    define_ClusterSink_methods(cls);
}
