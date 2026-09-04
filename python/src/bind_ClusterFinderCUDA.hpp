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

    using VT = typename ClusterType::value_type;
    constexpr size_t NPIX =
        static_cast<size_t>(ClusterSizeX) * static_cast<size_t>(ClusterSizeY);

    // Zero-copy view into the finder's pinned D2H buffer.
    py::class_<typename CF::BatchView>(m, (class_name + "_BatchView").c_str())
        .def_property_readonly("n_frames", &CF::BatchView::n_frames)
        .def_property_readonly("first_frame", &CF::BatchView::first_frame)
        .def_property_readonly("valid", &CF::BatchView::valid)
        .def_property_readonly("total_clusters", &CF::BatchView::total_clusters)
        .def("count", &CF::BatchView::count, py::arg("frame_index"))
        .def("release", &CF::BatchView::release,
             R"(Give the slot back to the finder. The view is unusable
             afterwards and submit_batch() may reuse the buffer. Called
             automatically on destruction and on __exit__.)")
        .def("__enter__", [](py::object self) { return self; })
        .def("__exit__", [](typename CF::BatchView &v, py::object, py::object,
                            py::object) { v.release(); })
        .def_property_readonly(
            "counts",
            [](const typename CF::BatchView &v) {
                py::array_t<uint32_t> out(v.n_frames());
                auto *o = out.mutable_data();
                for (size_t i = 0; i < v.n_frames(); ++i)
                    o[i] = v.count(i);
                return out;
            },
            R"(Clusters found per frame, as a numpy array.)")
        .def(
            "sums",
            [](const typename CF::BatchView &v) {
                std::vector<VT> s;
                {
                    // Reduce without the GIL, but take it back before building
                    // the array — constructing a Python object without it is a
                    // segfault, so this cannot be a call_guard.
                    py::gil_scoped_release nogil;
                    s = v.sums();
                }
                return py::array_t<VT>(s.size(), s.data());
            },
            R"(Per-cluster sums for the whole batch, reduced in C++ straight out
            of the pinned buffer — the clusters are never materialised on the
            host. This is the fast path for spectra/histograms.)")
        .def(
            "frame_data",
            [](py::object self, size_t i) {
                auto &v = self.cast<typename CF::BatchView &>();
                const uint32_t n = v.count(i);
                // Zero-copy (n, NPIX) view; stride skips each cluster's x/y.
                return py::array_t<VT>(
                    {static_cast<size_t>(n), NPIX},
                    {sizeof(ClusterType), sizeof(VT)},
                    n == 0 ? nullptr : v.clusters(i)->data.data(), self);
            },
            py::arg("frame_index"),
            R"(Zero-copy (n_clusters, ClusterSizeX*ClusterSizeY) view of one
            frame's pixel data, straight out of the pinned buffer. Valid until
            this view is released — copy it if you need to keep it.)")
        .def(
            "frame_xy",
            [](py::object self, size_t i) {
                auto &v = self.cast<typename CF::BatchView &>();
                const uint32_t n = v.count(i);
                return py::array_t<CoordType>(
                    {static_cast<size_t>(n), size_t{2}},
                    {sizeof(ClusterType), sizeof(CoordType)},
                    n == 0 ? nullptr : &v.clusters(i)->x, self);
            },
            py::arg("frame_index"),
            R"(Zero-copy (n_clusters, 2) view of one frame's cluster centre
            coordinates as (x, y).)");

    py::class_<CF>(m, class_name.c_str())
        .def(py::init<Shape<2>, float, size_t, int, bool>(),
             py::arg("image_size"), py::arg("n_sigma") = 5.0f,
             py::arg("max_clusters_per_frame") = 2048, py::arg("n_streams") = 4,
             py::arg("time_kernels") = false)

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
            "device_pedestal",
            [](CF &self, int stream) {
                auto pd = new NDArray<pd_type, 2>{};
                *pd = self.device_pedestal(stream);
                return return_image_data(pd);
            },
            py::arg("stream") = 0,
            R"(Device pedestal MEAN for a stream — the pedestal the kernel
actually decides with and updates each frame, unlike `pedestal` (the frozen
host pedestal). Read it before find_clusters() for the decision-time state.)")

        .def(
            "device_noise",
            [](CF &self, int stream) {
                auto arr = new NDArray<pd_type, 2>{};
                *arr = self.device_noise(stream);
                return return_image_data(arr);
            },
            py::arg("stream") = 0,
            R"(Device pedestal RMS for a stream, computed as the kernel does:
sqrt(max(sum2/n - mean^2, 0)). Counterpart to `noise` for the device pedestal.)")

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
            slot so it can be reused by the next submit_batch() call.

            One allocation and one copy per frame; see collect_view() for
            neither.)")

        .def(
            "collect_view",
            [](CF &self, typename CF::BatchToken token) {
                return self.collect_view(token);
            },
            py::arg("token"), py::call_guard<py::gil_scoped_release>(),
            R"(Like collect(), but copies nothing: returns a BatchView onto the
            finder's pinned D2H buffer.

            The slot stays reserved until the view is released, so with 2 slots
            you must finish with it before submitting two more batches. Use it
            as a context manager, or call release():

                tok = cf.submit_batch(frames[a:b], first_frame=a)
                with cf.collect_view(tok) as v:
                    hist.fill(v.sums())      # reduced in C++, nothing copied

            Anything you want to keep past release() must be copied out.)")

        .def("avg_kernel_time_ms", &CF::avg_kernel_time_ms,
             R"(Average per-frame kernel time in ms, or NaN if the finder was
            constructed with time_kernels=False (the default).

            WARNING: only meaningful with n_streams=1. The CUDA events bracket
            the kernel on its own stream, so under multi-stream contention the
            interval includes time spent queued behind other streams' kernels
            and over-reads by up to ~3.5x. Use Nsight Systems for exclusive
            kernel times.)")

        .def("kernel_timing_enabled", &CF::kernel_timing_enabled,
             R"(True if per-frame kernel timing was enabled at construction.)")

        .def("chunk_size_for", &CF::chunk_size_for, py::arg("n_frames"),
             R"(The chunk size find_clusters_batched() would use for n_frames.
             Use it to match its pipelining when driving submit_batch/
             collect_view by hand.)")

        .def("reserve_output_slots", &CF::reserve_output_slots,
             py::arg("n_frames"), py::call_guard<py::gil_scoped_release>(),
             R"(Pre-allocate both pinned output slots for batches of n_frames.

            Processes nothing: no transfer, no kernel, and the pedestal is NOT
            advanced. Only the two cudaMallocHost calls that the first
            submit_batch() would make happen here.

            Page-locking runs about 1.0 us per 4 kB page (~66 ms for two
            128 MiB slots), and that cost is charged to whoever triggers it.
            Call this before starting a timer so it does not land inside the
            measurement. Slots only grow, so pass the largest batch you intend
            to use:

                cf.reserve_output_slots(cf.chunk_size_for(len(data)))
            )")

        .def_property(
            "batch_chunk", &CF::get_batch_chunk, &CF::set_batch_chunk,
            R"(Frames per internally pipelined chunk in find_clusters_batched().

            0 (default) = auto: the batch is split into ~8 chunks so the host
            marshals one chunk while the GPU runs the next. Rounded up to a
            multiple of n_streams, which keeps the frame->stream assignment —
            and therefore the per-stream device pedestal each frame sees —
            identical to processing the batch in one go.

            Set equal to the batch size to disable chunking and recover the old
            submit-everything-then-copy-everything behaviour.)")

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