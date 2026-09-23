// SPDX-License-Identifier: MPL-2.0
#pragma once

#include "aare/MultiThreadedFileReader.hpp"

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <stdexcept>
#include <vector>

#include <pybind11/numpy.h>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include <pybind11/stl/filesystem.h>

namespace py = pybind11;

inline py::dtype multi_threaded_reader_numpy_dtype(const aare::Dtype &dtype) {
    using aare::Dtype;
    if (dtype == Dtype::INT8)
        return py::dtype::of<int8_t>();
    if (dtype == Dtype::UINT8)
        return py::dtype::of<uint8_t>();
    if (dtype == Dtype::INT16)
        return py::dtype::of<int16_t>();
    if (dtype == Dtype::UINT16)
        return py::dtype::of<uint16_t>();
    if (dtype == Dtype::INT32)
        return py::dtype::of<int32_t>();
    if (dtype == Dtype::UINT32)
        return py::dtype::of<uint32_t>();
    if (dtype == Dtype::INT64)
        return py::dtype::of<int64_t>();
    if (dtype == Dtype::UINT64)
        return py::dtype::of<uint64_t>();
    if (dtype == Dtype::FLOAT)
        return py::dtype::of<float>();
    if (dtype == Dtype::DOUBLE)
        return py::dtype::of<double>();
    throw std::runtime_error("Unsupported pixel data type");
}

inline py::array
multi_threaded_reader_read(aare::experimental::MultiThreadedFileReader &reader,
                           bool read_all) {
    const size_t n_frames =
        read_all ? reader.remaining_frames() : reader.next_read_frames();
    const std::vector<py::ssize_t> shape{
        static_cast<py::ssize_t>(n_frames),
        static_cast<py::ssize_t>(reader.rows()),
        static_cast<py::ssize_t>(reader.cols())};

    py::array image(multi_threaded_reader_numpy_dtype(reader.dtype()), shape);
    auto *destination = reinterpret_cast<std::byte *>(image.mutable_data());
    {
        py::gil_scoped_release release;
        if (read_all) {
            size_t offset = 0;
            while (reader.remaining_frames() != 0) {
                const size_t frames_read =
                    reader.read_into(destination + offset);
                offset += frames_read * reader.bytes_per_frame();
            }
        } else {
            reader.read_into(destination);
        }
    }
    return image;
}

inline void define_multi_threaded_file_reader_bindings(py::module_ &m) {
    using aare::experimental::MultiThreadedFileReader;

    auto reader =
        py::class_<MultiThreadedFileReader>(m, "MultiThreadedFileReader");
    reader.attr("__module__") = "aare.experimental";
    reader
        .def(py::init<std::filesystem::path, size_t, size_t,
                      std::optional<size_t>>(),
             py::arg("fname"), py::arg("n_threads"), py::arg("chunk_size"),
             py::arg("total_frames") = py::none(),
             R"doc(
                 Read chunks of detector frames concurrently.

                 Each worker opens an independent File. The returned array is
                 ordered by frame index even though chunks are read in parallel.

                 Args:
                     fname: Path accepted by File.
                     n_threads: Maximum number of worker threads.
                     chunk_size: Number of frames read per claimed chunk.
                     total_frames: Optional frame limit. None reads all frames.
                 )doc")
        .def(
            "read",
            [](MultiThreadedFileReader &self) {
                return multi_threaded_reader_read(self, false);
            },
            R"doc(
                 Read one chunk per active worker into a NumPy array.

                 Returns:
                     An array containing at most n_threads * chunk_size frames.
                     An empty array is returned at the end of the configured
                     frame range. The GIL is released while file data is read.
                 )doc")
        .def(
            "read_all",
            [](MultiThreadedFileReader &self) {
                return multi_threaded_reader_read(self, true);
            },
            R"doc(
                 Read all frames remaining from the current position.
                 )doc")
        .def_property_readonly("n_threads", &MultiThreadedFileReader::n_threads)
        .def_property_readonly("chunk_size",
                               &MultiThreadedFileReader::chunk_size)
        .def_property_readonly("total_frames",
                               &MultiThreadedFileReader::total_frames)
        .def_property_readonly("source_total_frames",
                               &MultiThreadedFileReader::source_total_frames)
        .def_property_readonly("rows", &MultiThreadedFileReader::rows)
        .def_property_readonly("cols", &MultiThreadedFileReader::cols)
        .def_property_readonly("bitdepth", &MultiThreadedFileReader::bitdepth)
        .def_property_readonly("dtype",
                               [](const MultiThreadedFileReader &self) {
                                   return multi_threaded_reader_numpy_dtype(
                                       self.dtype());
                               })
        .def_property_readonly("bytes_per_frame",
                               &MultiThreadedFileReader::bytes_per_frame)
        .def_property_readonly("total_bytes",
                               &MultiThreadedFileReader::total_bytes)
        .def_property_readonly("remaining_frames",
                               &MultiThreadedFileReader::remaining_frames)
        .def_property_readonly("next_read_frames",
                               &MultiThreadedFileReader::next_read_frames)
        .def_property_readonly("next_read_bytes",
                               &MultiThreadedFileReader::next_read_bytes)
        .def("seek", &MultiThreadedFileReader::seek, py::arg("frame_index"))
        .def("tell", &MultiThreadedFileReader::tell)
        .def("close", &MultiThreadedFileReader::close,
             "Close all worker files. Safe to call more than once.")
        .def_property_readonly(
            "closed",
            [](const MultiThreadedFileReader &self) { return !self.is_open(); })
        .def("__len__", &MultiThreadedFileReader::total_frames)
        .def(
            "__enter__",
            [](MultiThreadedFileReader &self) -> MultiThreadedFileReader * {
                if (!self.is_open()) {
                    throw std::runtime_error(
                        "Cannot enter a closed MultiThreadedFileReader");
                }
                return &self;
            },
            py::return_value_policy::reference_internal)
        .def("__exit__",
             [](MultiThreadedFileReader &self, const py::object &,
                const py::object &, const py::object &) {
                 self.close();
                 return false;
             })
        .def(
            "__iter__", [](MultiThreadedFileReader &self) { return &self; },
            py::return_value_policy::reference_internal)
        .def("__next__", [](MultiThreadedFileReader &self) {
            if (self.remaining_frames() == 0) {
                throw py::stop_iteration();
            }
            return multi_threaded_reader_read(self, false);
        });
}
