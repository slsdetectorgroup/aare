// SPDX-License-Identifier: MPL-2.0
#pragma once
#include "aare/ClusterFileSink.hpp"
#include "aare/ClusterFinder.hpp"
#include "aare/ClusterFinderFileSink.hpp"
#include "aare/ClusterFinderVectorSink.hpp"
#include "aare/ClusterVector.hpp"
#include "aare/ClusterVectorSink.hpp"
#include "aare/NDArray.hpp"
#include "aare/NDView.hpp"

#include "module_config.hpp"
#include "np_helper.hpp"

#include <cstdint>
#include <filesystem>
#include <fmt/format.h>
#include <pybind11/numpy.h>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include <pybind11/stl/filesystem.h>
#include <string>

namespace py = pybind11;

/**
 * Methods shared by every ClusterFinder<Sink>, independent of the sink type.
 * The constructors differ per sink and are added by the callers.
 */
template <typename Finder, uint8_t ClusterSizeX, uint8_t ClusterSizeY>
void define_ClusterFinder_methods(py::class_<Finder> &cls) {
    cls.def(
           "push_pedestal_frame",
           [](Finder &self, py::array_t<uint16_t> frame) {
               auto view = make_view_2d(frame);
               self.push_pedestal_frame(view);
           },
           py::arg("frame").noconvert())
        .def(
            "find_clusters",
            [](Finder &self, py::array_t<uint16_t> frame,
               uint64_t frame_number) {
                auto view = make_view_2d(frame);
                self.find_clusters(view, frame_number);
            },
            py::arg("frame").noconvert(), py::arg("frame_number") = 0)
        .def(
            "sync", [](Finder &self) { self.sync(); },
            py::call_guard<py::gil_scoped_release>(),
            R"(Wait until all queued frames have been processed and handed to the sink.)")
        .def(
            "stop", [](Finder &self) { self.stop(); },
            py::call_guard<py::gil_scoped_release>(),
            R"(Process all queued frames, then drain and stop the sink. For a file sink the file is complete afterwards. Calling it again does nothing.)")
        .def("clear_pedestal", [](Finder &self) { self.clear_pedestal(); })
        .def("update_threshold", [](Finder &self) { self.update_threshold(); })
        .def(
            "set_nSigma",
            [](Finder &self, pd_type nSigma) { self.set_nSigma(nSigma); },
            py::arg("nSigma"),
            R"(Set the number of sigma for all cluster finder threads.)")
        .def(
            "pedestal_ready",
            [](Finder &self) { return self.pedestal_ready(); },
            R"(Wait for the queued frames, then return True if all cluster finder threads have enough pedestal frames to find clusters.)")
        .def(
            "pedestal",
            [](Finder &self, size_t thread_index) {
                auto pd = new aare::NDArray<pd_type, 2>{};
                *pd = self.pedestal(thread_index);
                return return_image_data(pd);
            },
            py::arg("thread_index") = 0)
        .def(
            "noise",
            [](Finder &self, size_t thread_index) {
                auto arr = new aare::NDArray<pd_type, 2>{};
                *arr = self.noise(thread_index);
                return return_image_data(arr);
            },
            py::arg("thread_index") = 0)
        .def_property_readonly(
            "running", [](const Finder &self) { return self.running(); },
            R"(False once stop() has been called.)")
        .def_property_readonly("cluster_size",
                               []([[maybe_unused]] const Finder &self) {
                                   return py::make_tuple(ClusterSizeX,
                                                         ClusterSizeY);
                               })
        .def(
            "__enter__", [](Finder &self) -> Finder & { return self; },
            py::return_value_policy::reference)
        .def("__exit__", [](Finder &self, const py::object &,
                            const py::object &, const py::object &) {
            py::gil_scoped_release release;
            self.stop();
        });
}

/**
 * ClusterFinder with a ClusterVectorSink sink, exposed as
 * ClusterFinderVectorSink_<typestr>.
 */
template <typename T, uint8_t ClusterSizeX, uint8_t ClusterSizeY,
          typename CoordType = uint16_t>
void define_ClusterFinderVectorSink(py::module &m, const std::string &typestr) {
    auto class_name = fmt::format("ClusterFinderVectorSink_{}", typestr);

    using ClusterType = aare::Cluster<T, ClusterSizeX, ClusterSizeY, CoordType>;
    using Finder = aare::ClusterFinderVectorSink<ClusterType>;

    py::class_<Finder> cls(
        m, class_name.c_str(),
        R"(Multi-threaded cluster finder that keeps the clusters in memory.
The finder threads and the collector start on construction. Take the
clusters with get_clusters() (waits for queued frames) or
steal_clusters(). stop() or leaving a with block stops both.)");
    cls.def(py::init<aare::Shape<2>, pd_type, size_t, size_t, size_t, size_t>(),
            py::arg("image_size"), py::arg("n_sigma") = 5.0,
            py::arg("capacity") = 2048, py::arg("n_threads") = 3,
            py::arg("queue_depth") = 16,
            py::arg("min_pedestal_samples") = 1000);
    cls.def(
           "get_clusters", [](Finder &self) { return self.get_clusters(); },
           R"(Wait for all queued frames to be processed, then return all clusters currently accumulated in memory.)")
        .def(
            "steal_clusters",
            [](Finder &self) { return self.steal_clusters(); },
            R"(Return all clusters currently accumulated in memory. Does not wait for queued frames to be processed.)");
    define_ClusterFinder_methods<Finder, ClusterSizeX, ClusterSizeY>(cls);
}

/**
 * ClusterFinder with a ClusterFileSink sink, exposed as
 * ClusterFinderFileSink_<typestr>.
 */
template <typename T, uint8_t ClusterSizeX, uint8_t ClusterSizeY,
          typename CoordType = uint16_t>
void define_ClusterFinderFileSink(py::module &m, const std::string &typestr) {
    auto class_name = fmt::format("ClusterFinderFileSink_{}", typestr);

    using ClusterType = aare::Cluster<T, ClusterSizeX, ClusterSizeY, CoordType>;
    using Finder = aare::ClusterFinder<aare::ClusterFileSink<ClusterType>,
                                       uint16_t, pd_type>;

    py::class_<Finder> cls(
        m, class_name.c_str(),
        R"(Multi-threaded cluster finder that writes the clusters to a binary
cluster file. The file is replaced if it exists and is complete once
stop() has returned or a with block is left. Raises RuntimeError if the
file cannot be opened.)");
    cls.def(py::init([](aare::Shape<2> image_size,
                        const std::filesystem::path &fname, pd_type n_sigma,
                        size_t capacity, size_t n_threads, size_t queue_depth,
                        size_t min_pedestal_samples) {
                return new Finder(image_size, n_sigma, capacity, n_threads,
                                  queue_depth, min_pedestal_samples, fname);
            }),
            py::arg("image_size"), py::arg("fname"), py::arg("n_sigma") = 5.0,
            py::arg("capacity") = 2048, py::arg("n_threads") = 3,
            py::arg("queue_depth") = 16,
            py::arg("min_pedestal_samples") = 1000);
    define_ClusterFinder_methods<Finder, ClusterSizeX, ClusterSizeY>(cls);
}
