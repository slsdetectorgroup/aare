// SPDX-License-Identifier: MPL-2.0
#pragma once
#include "aare/ClusterFinderMT.hpp"
#include "aare/ClusterSink.hpp"
#include "aare/ClusterVector.hpp"

#include "module_config.hpp"

#include <cstdint>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

namespace py = pybind11;

/**
 * Add the methods every ClusterSink provides (collect, stop,
 * steal_clusters) to the binding of a concrete sink. ClusterSink is a CRTP
 * base, so it has no Python class of its own; each sink binding calls this
 * instead.
 */
template <typename Sink>
void define_ClusterSink_methods(py::class_<Sink> &cls) {
    static_assert(aare::is_cluster_sink_v<Sink>,
                  "Sink must derive from aare::ClusterSink");
    using Finder =
        aare::ClusterFinderMT<typename Sink::cluster_type, uint16_t, pd_type>;

    cls.def(
           "collect",
           [](Sink &self, Finder &finder) { self.collect(finder.sink()); },
           py::arg("finder"),
           // The sink thread reads the output queue of the finder, so the
           // finder must outlive the sink.
           py::keep_alive<1, 2>(),
           R"(Start the sink thread reading the output queue of a ClusterFinderMT. Does nothing if the sink is already running.)")
        .def(
            "stop", [](Sink &self) { self.stop(); },
            // Joining the sink thread can wait; it never needs the GIL.
            py::call_guard<py::gil_scoped_release>(),
            R"(
            Stop the sink thread once the output queue of the finder is empty.
            Does nothing if the sink is already stopped.)");
}
