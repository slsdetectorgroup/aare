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
#include "state_file.hpp"

#include <cstdint>
#include <filesystem>
#include <memory>
#include <optional>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include <pybind11/stl_bind.h>
#include <string>

namespace py = pybind11;

using namespace aare;

template <typename T, uint8_t ClusterSizeX, uint8_t ClusterSizeY,
          typename CoordType = uint16_t>
void define_ClusterFinderMT(py::module &m, const std::string &typestr) {
    auto class_name = fmt::format("ClusterFinderMT_{}", typestr);

    using ClusterType = Cluster<T, ClusterSizeX, ClusterSizeY, CoordType>;

    using Finder = ClusterFinderMT<ClusterType, uint16_t, pd_type>;

    py::class_<Finder> cls(m, class_name.c_str());
    cls.def(py::init<Shape<2>, pd_type, size_t, size_t, size_t, size_t>(),
            py::arg("image_size"), py::arg("n_sigma") = 5.0,
            py::arg("capacity") = 2048, py::arg("n_threads") = 3,
            py::arg("queue_depth") = 16, py::arg("min_pedestal_samples") = 1000)
        .def(
            "push_pedestal_frame",
            [](ClusterFinderMT<ClusterType, uint16_t, pd_type> &self,
               py::array_t<uint16_t> frame) {
                auto view = make_view_2d(frame);
                self.push_pedestal_frame(view);
            },
            py::arg("frame").noconvert())
        .def(
            "find_clusters",
            [](ClusterFinderMT<ClusterType, uint16_t, pd_type> &self,
               py::array_t<uint16_t> frame, uint64_t frame_number) {
                auto view = make_view_2d(frame);
                self.find_clusters(view, frame_number);
            },
            py::arg("frame").noconvert(), py::arg("frame_number") = 0)
        .def_property_readonly(
            "cluster_size",
            []([[maybe_unused]] ClusterFinderMT<ClusterType, uint16_t, pd_type>
                   &self) {
                return py::make_tuple(ClusterSizeX, ClusterSizeY);
            })
        .def("clear_pedestal",
             &ClusterFinderMT<ClusterType, uint16_t, pd_type>::clear_pedestal)
        .def("update_threshold",
             &ClusterFinderMT<ClusterType, uint16_t, pd_type>::update_threshold)
        .def(
            "pedestal_ready",
            &ClusterFinderMT<ClusterType, uint16_t, pd_type>::pedestal_ready,
            R"(waits for the queued frames, then returns True if all cluster finders have enough pedestal frames to find clusters.)")
        .def("sync", &ClusterFinderMT<ClusterType, uint16_t, pd_type>::sync)
        .def("stop", &ClusterFinderMT<ClusterType, uint16_t, pd_type>::stop)
        .def("start", &ClusterFinderMT<ClusterType, uint16_t, pd_type>::start)
        .def(
            "pedestal",
            [](ClusterFinderMT<ClusterType, uint16_t, pd_type> &self,
               size_t thread_index) {
                auto pd = new NDArray<pd_type, 2>{};
                *pd = self.pedestal(thread_index);
                return return_image_data(pd);
            },
            py::arg("thread_index") = 0)
        .def(
            "noise",
            [](ClusterFinderMT<ClusterType, uint16_t, pd_type> &self,
               size_t thread_index) {
                auto arr = new NDArray<pd_type, 2>{};
                *arr = self.noise(thread_index);
                return return_image_data(arr);
            },
            py::arg("thread_index") = 0)

        .def("set_nSigma",
             &ClusterFinderMT<ClusterType, uint16_t, pd_type>::set_nSigma,
             py::arg("nSigma"),
             R"(sets the number of sigma for all cluster finders.)");

    cls.def(
        "save_state",
        [](const Finder &self, const std::filesystem::path &filename) {
            aare::save_state(self.state(), filename);
        },
        py::arg("filename"),
        "Write the complete state of the cluster finder to a state file: "
        "for each thread its pedestal, nSigma and threshold, and which "
        "thread takes the next frame, after the frames given so far. An "
        "existing file "
        "is replaced. Clusters that have not reached the sink are not "
        "stored.");
    // An instance method: as a static method that returns a new finder it
    // could be called on an existing one, which then stayed unchanged
    cls.def(
        "load_state",
        [](Finder &self, const std::filesystem::path &filename,
           std::optional<size_t> thread_index) {
            // The file of a single-threaded finder holds one state, which
            // goes to one thread or to all
            const auto header = py::module_::import("json").attr("loads")(
                aare::read_state_header(filename));
            if (header.attr("get")("type").equal(py::str("ClusterFinder"))) {
                self.set_state(
                    aare::load_state<aare::ClusterFinderState>(filename),
                    thread_index);
                return;
            }
            if (thread_index) {
                throw std::runtime_error(
                    "thread_index can only be given with the state file of "
                    "a ClusterFinder; the state file of a ClusterFinderMT "
                    "holds the state of every thread");
            }
            self.set_state(
                aare::load_state<aare::ClusterFinderMTState>(filename));
        },
        py::arg("filename"), py::arg("thread_index") = py::none(),
        "Replace the state of this cluster finder with the one in a state "
        "file. Frames given before use the previous state. The file of a "
        "ClusterFinderMT must have "
        "been written with the number of threads of this finder and "
        "replaces the state of every thread. The file of a ClusterFinder "
        "holds one state, which is given to the thread thread_index or, "
        "without it, to every thread. The cluster size and the image shape "
        "must be those of this finder, which keeps its dtype. If the file "
        "cannot be loaded the finder is left unchanged.");
    cls.def_static(
        "from_state_file",
        [](const std::filesystem::path &filename, size_t capacity,
           size_t queue_depth) {
            return std::make_unique<Finder>(
                aare::load_state<aare::ClusterFinderMTState>(filename),
                capacity, queue_depth);
        },
        py::arg("filename"), py::arg("capacity") = 2048,
        py::arg("queue_depth") = 16,
        "Create a cluster finder of this cluster size and dtype from the "
        "state file of a ClusterFinderMT, with the number of threads "
        "stored in it, and start its threads.");

    const char *set_pedestal_doc =
        "Replace the pedestal of the thread thread_index or, without it, "
        "of every thread with a copy of a FastPedestal of any dtype with "
        "the same image shape. Frames given before use the previous "
        "pedestal. nSigma is kept, and the "
        "threshold of each thread that gets the pedestal is recomputed as "
        "noise times nSigma if the pedestal is ready, otherwise when "
        "pedestal frames make it ready.";
    for_each_fast_pedestal_type([&](auto type) {
        using Pedestal = typename decltype(type)::type;
        cls.def(
            "set_pedestal",
            [](Finder &self, const Pedestal &pedestal,
               std::optional<size_t> thread_index) {
                self.set_pedestal(pedestal, thread_index);
            },
            py::arg("pedestal"), py::arg("thread_index") = py::none(),
            set_pedestal_doc);
    });
    cls.def(
        "get_pedestal",
        [](const Finder &self, size_t thread_index) {
            return self.get_pedestal(thread_index);
        },
        py::arg("thread_index") = 0,
        "Return a copy of the pedestal of one thread as a FastPedestal. "
        "On a running finder the frames given so far are included. The "
        "pedestals of the threads differ once "
        "clusters have been found.");
    cls.def(
        "load_pedestal",
        [](Finder &self, const std::filesystem::path &filename,
           std::optional<size_t> thread_index) {
            self.set_pedestal(
                aare::load_state<aare::FastPedestalState>(filename),
                thread_index);
        },
        py::arg("filename"), py::arg("thread_index") = py::none(),
        "Replace the pedestal of the thread thread_index or, without it, "
        "of every thread with the one in the state file of a FastPedestal, "
        "as set_pedestal() does with a FastPedestal.");
    cls.def(
        "save_pedestal",
        [](const Finder &self, const std::filesystem::path &filename,
           size_t thread_index) {
            aare::save_state(self.get_pedestal(thread_index).state(), filename);
        },
        py::arg("filename"), py::arg("thread_index") = 0,
        "Write the pedestal of one thread to a state file of a "
        "FastPedestal, which FastPedestal.load_state() and load_pedestal() "
        "read. An existing file is replaced.");
    cls.def_property_readonly(
        "n_threads", [](const Finder &self) { return self.n_threads(); },
        "Number of processing threads.");
}
