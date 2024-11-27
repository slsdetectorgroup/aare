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

void define_raw_file_io_bindings(py::module &m) {
    py::class_<RawFile>(m, "RawFile")
        .def(py::init<const std::filesystem::path &>())
        .def("read_frame",
             [](RawFile &self) {
                 py::array image;
                 std::vector<ssize_t> shape;
                 shape.reserve(2);
                 shape.push_back(self.rows());
                 shape.push_back(self.cols());

                 // return headers from all subfiles
                 py::array_t<DetectorHeader> header(self.n_mod());

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
                     header.mutable_data());

                 return py::make_tuple(header, image);
             })
        .def(
            "read_n",
            [](RawFile &self, size_t n_frames) {
                // adjust for actual frames left in the file
                n_frames =
                    std::min(n_frames, self.total_frames() - self.tell());
                if (n_frames == 0) {
                    throw std::runtime_error("No frames left in file");
                }
                std::vector<size_t> shape{n_frames, self.rows(), self.cols()};
            
                // return headers from all subfiles
                py::array_t<DetectorHeader> header;
                if (self.n_mod() == 1) {
                    header = py::array_t<DetectorHeader>(n_frames);
                } else {
                    header = py::array_t<DetectorHeader>({self.n_mod(), n_frames});
                }
                // py::array_t<DetectorHeader> header({self.n_mod(), n_frames});

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
                    n_frames, header.mutable_data());

                return py::make_tuple(header, image);
            },
            R"(
             Read n frames from the file.
             )")
        .def("frame_number", &RawFile::frame_number)
        .def_property_readonly("bytes_per_frame", &RawFile::bytes_per_frame)
        .def_property_readonly("pixels_per_frame", &RawFile::pixels_per_frame)
        .def_property_readonly("bytes_per_pixel", &RawFile::bytes_per_pixel)
        .def("seek", &RawFile::seek, R"(
            Seek to a frame index in file.
            )")
        .def("tell", &RawFile::tell, R"(
            Return the current frame number.)")
        .def_property_readonly("total_frames", &RawFile::total_frames)
        .def_property_readonly("rows", &RawFile::rows)
        .def_property_readonly("cols", &RawFile::cols)
        .def_property_readonly("bitdepth", &RawFile::bitdepth)
        .def_property_readonly("geometry", &RawFile::geometry)
        .def_property_readonly("n_mod", &RawFile::n_mod)
        .def_property_readonly("detector_type", &RawFile::detector_type)
        .def_property_readonly("master", &RawFile::master);
}