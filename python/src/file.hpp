// SPDX-License-Identifier: MPL-2.0
#include "aare/CtbRawFile.hpp"
#include "aare/File.hpp"
#include "aare/Frame.hpp"
#include "aare/ROI.hpp"
#include "aare/RawFile.hpp"
#include "aare/RawMasterFile.hpp"
#include "aare/RawSubFile.hpp"

#include "aare/defs.hpp"

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

// Disable warnings for unused parameters, as we ignore some
// in the __exit__ method
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"

void define_file_io_bindings(py::module &m) {

    PYBIND11_NUMPY_DTYPE(DetectorHeader, frameNumber, expLength, packetNumber,
                         bunchId, timestamp, modId, row, column, reserved,
                         debug, roundRNumber, detType, version, packetMask);

    py::class_<File>(m, "File")
        .def(py::init([](const std::filesystem::path &fname) {
            return File(fname, "r", {});
        }))
        .def(py::init(
            [](const std::filesystem::path &fname, const std::string &mode) {
                return File(fname, mode, {});
            }))
        .def(py::init<const std::filesystem::path &, const std::string &,
                      const FileConfig &>())

        .def("frame_number", py::overload_cast<>(&File::frame_number))
        .def("frame_number", py::overload_cast<size_t>(&File::frame_number))
        .def_property_readonly("bytes_per_frame", &File::bytes_per_frame)
        .def_property_readonly("pixels_per_frame", &File::pixels_per_frame)
        .def("seek", &File::seek)
        .def("tell", &File::tell)
        .def_property_readonly("total_frames", &File::total_frames)
        .def("__len__", &File::total_frames)
        .def_property_readonly("rows", &File::rows)
        .def_property_readonly("cols", &File::cols)
        .def_property_readonly("bitdepth", &File::bitdepth)
        .def_property_readonly("bytes_per_pixel", &File::bytes_per_pixel)
        .def_property_readonly("detector_type",
                               [](File &self) { return self.detector_type(); })
        .def("read_frame",
             [](File &self) {
                 const uint8_t item_size = self.bytes_per_pixel();
                 py::array image;
                 std::vector<ssize_t> shape;
                 shape.reserve(2);
                 shape.push_back(self.rows());
                 shape.push_back(self.cols());
                 if (item_size == 1) {
                     image = py::array_t<uint8_t>(shape);
                 } else if (item_size == 2) {
                     image = py::array_t<uint16_t>(shape);
                 } else if (item_size == 4) {
                     image = py::array_t<uint32_t>(shape);
                 }
                 self.read_into(
                     reinterpret_cast<std::byte *>(image.mutable_data()));
                 return image;
             })
        .def("read_frame",
             [](File &self, size_t frame_number) {
                 self.seek(frame_number);
                 const uint8_t item_size = self.bytes_per_pixel();
                 py::array image;
                 std::vector<ssize_t> shape;
                 shape.reserve(2);
                 shape.push_back(self.rows());
                 shape.push_back(self.cols());
                 if (item_size == 1) {
                     image = py::array_t<uint8_t>(shape);
                 } else if (item_size == 2) {
                     image = py::array_t<uint16_t>(shape);
                 } else if (item_size == 4) {
                     image = py::array_t<uint32_t>(shape);
                 }
                 self.read_into(
                     reinterpret_cast<std::byte *>(image.mutable_data()));
                 return image;
             })
        .def("read_n",
             [](File &self, size_t n_frames) {
                 // adjust for actual frames left in the file
                 n_frames =
                     std::min(n_frames, self.total_frames() - self.tell());
                 if (n_frames == 0) {
                     throw std::runtime_error("No frames left in file");
                 }
                 std::vector<size_t> shape{n_frames, self.rows(), self.cols()};

                 py::array image;
                 const uint8_t item_size = self.bytes_per_pixel();
                 if (item_size == 1) {
                     image = py::array_t<uint8_t>(shape);
                 } else if (item_size == 2) {
                     image = py::array_t<uint16_t>(shape);
                 } else if (item_size == 4) {
                     image = py::array_t<uint32_t>(shape);
                 }
                 self.read_into(
                     reinterpret_cast<std::byte *>(image.mutable_data()),
                     n_frames);
                 return image;
             })
        .def("__enter__", [](File &self) { return &self; })
        .def("__exit__",
             [](File &self, const std::optional<pybind11::type> &exc_type,
                const std::optional<pybind11::object> &exc_value,
                const std::optional<pybind11::object> &traceback) {
                 //  self.close();
             })
        .def("__iter__", [](File &self) { return &self; })
        .def("__next__", [](File &self) {
            try {
                const uint8_t item_size = self.bytes_per_pixel();
                py::array image;
                std::vector<ssize_t> shape;
                shape.reserve(2);
                shape.push_back(self.rows());
                shape.push_back(self.cols());
                if (item_size == 1) {
                    image = py::array_t<uint8_t>(shape);
                } else if (item_size == 2) {
                    image = py::array_t<uint16_t>(shape);
                } else if (item_size == 4) {
                    image = py::array_t<uint32_t>(shape);
                }
                self.read_into(
                    reinterpret_cast<std::byte *>(image.mutable_data()));
                return image;
            } catch (std::runtime_error &e) {
                throw py::stop_iteration();
            }
        });

    py::class_<ScanParameters>(m, "ScanParameters")
        .def(py::init<const std::string &>())
        .def(py::init<const ScanParameters &>())

        .def_property_readonly("enabled", &ScanParameters::enabled)
        .def_property_readonly("dac", &ScanParameters::dac)
        .def_property_readonly("start", &ScanParameters::start)
        .def_property_readonly("stop", &ScanParameters::stop)
        .def_property_readonly("step", &ScanParameters::step);

#pragma GCC diagnostic pop
}