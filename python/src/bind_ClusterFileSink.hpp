// SPDX-License-Identifier: MPL-2.0
#pragma once
#include "aare/ClusterFileSink.hpp"

#include "bind_ClusterSinkBase_methods.hpp"

#include <cstdint>
#include <filesystem>
#include <fmt/format.h>
#include <pybind11/pybind11.h>
#include <pybind11/stl/filesystem.h>
#include <string>

namespace py = pybind11;

template <typename T, uint8_t ClusterSizeX, uint8_t ClusterSizeY,
          typename CoordType = uint16_t>
void define_ClusterFileSink(py::module &m, const std::string &typestr) {
    auto class_name = fmt::format("ClusterFileSink_{}", typestr);

    using ClusterType = aare::Cluster<T, ClusterSizeX, ClusterSizeY, CoordType>;
    using Sink = aare::ClusterFileSink<ClusterType>;

    py::class_<Sink> cls(m, class_name.c_str(),
                         R"(ClusterSink that writes the clusters to a binary
cluster file. Start it with collect(). The file is complete once stop()
has returned. steal_clusters() returns nothing.)");
    cls.def(py::init<const std::filesystem::path &>(), py::arg("fname"),
            R"(Construct a ClusterFileSink that writes to the given file.)");
    // bind Base class methods
    define_ClusterSink_methods(cls);
}
