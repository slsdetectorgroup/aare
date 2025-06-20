#include "aare/CtbRawFile.hpp"
#include "aare/File.hpp"
#include "aare/Frame.hpp"
#include "aare/RawFile.hpp"
#include "aare/RawMasterFile.hpp"
#include "aare/RawSubFile.hpp"

#include "aare/defs.hpp"
// #include "aare/fClusterFileV2.hpp"

#include <cstdint>
#include <filesystem>
#include <pybind11/iostream.h>
#include <pybind11/numpy.h>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include <pybind11/stl/filesystem.h>
#include <string>

namespace py = pybind11;
using namespace ::aare;

auto read_frame_from_RawSubFile(RawSubFile &self) {
    py::array_t<DetectorHeader> header(1);
    const uint8_t item_size = self.bytes_per_pixel();
    std::vector<ssize_t> shape{static_cast<ssize_t>(self.rows()),
                               static_cast<ssize_t>(self.cols())};

    py::array image;
    if (item_size == 1) {
        image = py::array_t<uint8_t>(shape);
    } else if (item_size == 2) {
        image = py::array_t<uint16_t>(shape);
    } else if (item_size == 4) {
        image = py::array_t<uint32_t>(shape);
    }
    self.read_into(reinterpret_cast<std::byte *>(image.mutable_data()),
                   header.mutable_data());

    return py::make_tuple(header, image);
}

auto read_n_frames_from_RawSubFile(RawSubFile &self, size_t n_frames) {
    py::array_t<DetectorHeader> header(n_frames);
    const uint8_t item_size = self.bytes_per_pixel();
    std::vector<ssize_t> shape{static_cast<ssize_t>(n_frames),
                               static_cast<ssize_t>(self.rows()),
                               static_cast<ssize_t>(self.cols())};

    py::array image;
    if (item_size == 1) {
        image = py::array_t<uint8_t>(shape);
    } else if (item_size == 2) {
        image = py::array_t<uint16_t>(shape);
    } else if (item_size == 4) {
        image = py::array_t<uint32_t>(shape);
    }
    self.read_into(reinterpret_cast<std::byte *>(image.mutable_data()),
                   n_frames, header.mutable_data());

    return py::make_tuple(header, image);
}

// Disable warnings for unused parameters, as we ignore some
// in the __exit__ method
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"

void define_raw_sub_file_io_bindings(py::module &m) {
    py::class_<RawSubFile>(m, "RawSubFile")
        .def(py::init<const std::filesystem::path &, DetectorType, size_t,
                      size_t, size_t>())
        .def_property_readonly("bytes_per_frame", &RawSubFile::bytes_per_frame)
        .def_property_readonly("pixels_per_frame",
                               &RawSubFile::pixels_per_frame)
        .def_property_readonly("bytes_per_pixel", &RawSubFile::bytes_per_pixel)
        .def("seek", &RawSubFile::seek)
        .def("tell", &RawSubFile::tell)
        .def_property_readonly("rows", &RawSubFile::rows)
        .def_property_readonly("cols", &RawSubFile::cols)
        .def_property_readonly("frames_in_file", &RawSubFile::frames_in_file)
        .def("read_frame", &read_frame_from_RawSubFile)
        .def("read_n", &read_n_frames_from_RawSubFile)
        .def("read",
             [](RawSubFile &self) {
                 self.seek(0);
                 auto n_frames = self.frames_in_file();
                 return read_n_frames_from_RawSubFile(self, n_frames);
             })
        .def("__enter__", [](RawSubFile &self) { return &self; })
        .def("__exit__",
             [](RawSubFile &self, const std::optional<pybind11::type> &exc_type,
                const std::optional<pybind11::object> &exc_value,
                const std::optional<pybind11::object> &traceback) {})
        .def("__iter__", [](RawSubFile &self) { return &self; })
        .def("__next__", [](RawSubFile &self) {
            try {
                return read_frame_from_RawSubFile(self);
            } catch (std::runtime_error &e) {
                throw py::stop_iteration();
            }
        });
}

#pragma GCC diagnostic pop