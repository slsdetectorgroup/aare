// SPDX-License-Identifier: MPL-2.0
#pragma once
#include "aare/ClusterCollector.hpp"
#include "aare/ClusterFileSink.hpp"
#include "aare/ClusterFinder.hpp"
#include "aare/ClusterFinderMT.hpp"
#include "aare/ClusterVector.hpp"
#include "aare/NDView.hpp"
#include "aare/Pedestal.hpp"

#include "module_config.hpp"
#include "np_helper.hpp"

#include <cstdint>
#include <filesystem>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include <pybind11/stl_bind.h>

namespace py = pybind11;

using namespace aare;

template <typename T, uint8_t ClusterSizeX, uint8_t ClusterSizeY,
          typename CoordType = uint16_t>
void define_ClusterCollector(py::module &m, const std::string &typestr) {
    auto class_name = fmt::format("ClusterCollector_{}", typestr);

    using ClusterType = Cluster<T, ClusterSizeX, ClusterSizeY, CoordType>;

    py::class_<ClusterCollector<ClusterType>>(m, class_name.c_str())
        // The collector thread reads the sink of the finder
        .def(py::init<ClusterFinderMT<ClusterType, uint16_t, pd_type> *>(),
             py::keep_alive<1, 2>())
        .def("start", &ClusterCollector<ClusterType>::start,
             "Start the collector thread. Does nothing if it is running.")
        .def("stop", &ClusterCollector<ClusterType>::stop,
             "Stop the collector thread once all clusters in the sink of "
             "the finder are collected. Does nothing if it is stopped.")
        .def(
            "steal_clusters",
            [](ClusterCollector<ClusterType> &self) {
                auto v = new std::vector<ClusterVector<ClusterType>>(
                    self.steal_clusters());
                return v; // TODO change!!!
            },
            py::return_value_policy::take_ownership,
            "Take the clusters collected since the last call, one "
            "ClusterVector per frame. Can be called while the collector is "
            "running; call sync() on the finder first to get the clusters "
            "of every frame given to it so far.");
}
