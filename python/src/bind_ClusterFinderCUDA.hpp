// SPDX-License-Identifier: MPL-2.0
#pragma once
#include "aare/ClusterFinderCUDA.hpp"
#include "aare/ClusterVector.hpp"
#include "aare/NDView.hpp"
#include "aare/Pedestal.hpp"
#include "np_helper.hpp"

#include <cstdint>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
// #include <pybind11/stl_bind.h>

namespace py = pybind11;
using pd_type = double;

using namespace aare;

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"

namespace aare {

template <typename T, uint8_t ClusterSizeX, uint8_t ClusterSizeY,
          typename CoordType = uint16_t>
void define_ClusterFinderCUDA(py::module &m, const std::string &typestr) {
    auto class_name = fmt::format("ClusterFinderCUDA_{}", typestr);

    using ClusterType = Cluster<T, ClusterSizeX, ClusterSizeY, CoordType>;
    using CF = ClusterFinderCUDA<ClusterType, uint16_t, pd_type>;
    using ContigArr =
        py::array_t<uint16_t, py::array::c_style | py::array::forcecast>;

    // Opaque batch handle returned by submit_batch() and consumed by collect()
    py::class_<typename CF::BatchToken>(m,
                                        (class_name + "_BatchToken").c_str());

    py::class_<CF>(m, class_name.c_str())
        .def(py::init<Shape<2>, float, size_t, int>(), py::arg("image_size"),
             py::arg("n_sigma") = 5.0f,
             py::arg("max_clusters_per_frame") = 2048, py::arg("n_streams") = 4)

        .def_property(
            "nSigma", &CF::get_nSigma, &CF::set_nSigma,
            R"(Number of sigma above the pedestal to consider a photon during cluster finding.)")

        .def("push_pedestal_frame",
             [](CF &self, ContigArr frame) {
                 auto view = make_view_2d(frame);
                 self.push_pedestal_frame(view);
             })

        .def("clear_pedestal", &CF::clear_pedestal)

        .def_property_readonly("pedestal",
                               [](CF &self) {
                                   auto pd = new NDArray<pd_type, 2>{};
                                   *pd = self.pedestal();
                                   return return_image_data(pd);
                               })

        .def_property_readonly("noise",
                               [](CF &self) {
                                   auto arr = new NDArray<pd_type, 2>{};
                                   *arr = self.noise();
                                   return return_image_data(arr);
                               })

        .def(
            "steal_clusters",
            [](CF &self, bool realloc_same_capacity) {
                return std::move(self.steal_clusters(realloc_same_capacity));
            },
            py::arg("realloc_same_capacity") = true)

        .def(
            "find_clusters",
            [](CF &self, ContigArr frame, uint64_t frame_number) {
                auto view = make_view_2d(frame);
                self.find_clusters(view, frame_number);
            },
            py::arg("frame"), py::arg("frame_number") = 0,
            py::call_guard<py::gil_scoped_release>())

        .def(
            "find_clusters_batched",
            [](CF &self, ContigArr frames, uint64_t first_frame) {
                auto view = make_view_3d(frames);
                return self.find_clusters_batched(view, first_frame);
            },
            py::arg("frames"), py::arg("first_frame") = 0,
            py::call_guard<py::gil_scoped_release>(),
            R"(Process a 3D array of frames (n_frames, nrows, ncols) using
            n_streams CUDA streams for H2D/kernel/D2H pipelining. Returns a
            list of ClusterVector, one per input frame. The input array is
            converted to C-contiguous uint16 if needed.)")

        .def(
            "submit_batch",
            [](CF &self, ContigArr frames, uint64_t first_frame) {
                auto view = make_view_3d(frames);
                return self.submit_batch(view, first_frame);
            },
            py::arg("frames"), py::arg("first_frame") = 0,
            py::call_guard<py::gil_scoped_release>(),
            R"(Enqueue one batch of frames onto the GPU without waiting for
            completion. Returns a BatchToken that must be passed to collect()
            to retrieve results and release the slot.

            At most 2 batches can be in flight simultaneously. The intended
            usage pattern to eliminate inter-batch GPU idle time is:

                tok = cf.submit_batch(buf_a, first_frame=0)
                for start in range(BATCH_SIZE, N, BATCH_SIZE):
                    buf_b[:n] = data[start:start+n]       # fill next buffer
                    next_tok = cf.submit_batch(buf_b, first_frame=start)
                    results += cf.collect(tok)             # GPU runs buf_b
                    tok = next_tok
                    buf_a, buf_b = buf_b, buf_a            # swap
                results += cf.collect(tok)                 # drain last batch

            Two separate input buffers must be used (one per in-flight batch)
            so that filling the next buffer does not corrupt the ongoing H2D
            transfer for the current batch.)")

        .def(
            "collect",
            [](CF &self, typename CF::BatchToken token) {
                return self.collect(token);
            },
            py::arg("token"), py::call_guard<py::gil_scoped_release>(),
            R"(Wait for a previously submitted batch and return its results as
            a list of ClusterVector, one per input frame. Releases the batch
            slot so it can be reused by the next submit_batch() call.)")

        .def("avg_kernel_time_ms", &CF::avg_kernel_time_ms,
             R"(Average kernel execution time per frame in milliseconds,
            excluding PCIe transfers.)")

        .def("reset_timers", &CF::reset_timers,
             R"(Reset the internal kernel timing counters.)")

        .def(
            "register_input_buffer",
            [](CF &self, py::array arr) {
                auto info = arr.request();
                self.register_input_buffer(
                    info.ptr, static_cast<size_t>(info.size) *
                                  static_cast<size_t>(info.itemsize));
            },
            R"(Pin a numpy array as a locked host buffer so that
            find_clusters_batched transfers it at full DMA bandwidth
            (~22 GB/s) instead of going through the CUDA driver's
            internal staging (~15 GB/s for pageable memory).

            Call once before the processing loop with the full data
            array.  Slices of that array passed to find_clusters_batched
            lie within the registered region and benefit automatically.
            Call unregister_input_buffer() when done.)")

        .def("unregister_input_buffer", &CF::unregister_input_buffer,
             "Release the previously pinned input buffer.")

        .def(
            "pin_buffer",
            [](CF & /*self*/, py::array arr) {
                auto info = arr.request();
                CUDA_CHECK(
                    cudaHostRegister(info.ptr,
                                     static_cast<size_t>(info.size) *
                                         static_cast<size_t>(info.itemsize),
                                     cudaHostRegisterDefault));
            },
            R"(Pin an arbitrary numpy array as a locked host buffer for DMA-speed
            transfers. Unlike register_input_buffer(), does not unpin a
            previously registered buffer — use this to pin multiple buffers
            simultaneously (e.g. the two alternating buffers in an async
            pipeline). Call unpin_buffer() on each array when done.)")

        .def(
            "unpin_buffer",
            [](CF & /*self*/, py::array arr) {
                auto info = arr.request();
                CUDA_CHECK(cudaHostUnregister(info.ptr));
            },
            "Release a buffer previously pinned with pin_buffer().");
}

} // namespace aare

#pragma GCC diagnostic pop