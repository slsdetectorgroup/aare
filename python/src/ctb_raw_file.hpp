
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

void define_ctb_raw_file_io_bindings(py::module &m) {

 py::class_<CtbRawFile>(m, "CtbRawFile")
        .def(py::init<const std::filesystem::path &>())
        .def("read_frame",
             [](CtbRawFile &self) {
                 size_t image_size = self.image_size_in_bytes();
                 py::array image;
                 std::vector<ssize_t> shape;
                 shape.reserve(2);
                 shape.push_back(1);
                 shape.push_back(image_size);

                 py::array_t<DetectorHeader> header(1);

                 // always read bytes
                 image = py::array_t<uint8_t>(shape);

                 self.read_into(
                     reinterpret_cast<std::byte *>(image.mutable_data()),
                     header.mutable_data());

                 return py::make_tuple(header, image);
             })
        .def("seek", &CtbRawFile::seek)
        .def("tell", &CtbRawFile::tell)
        .def("master", &CtbRawFile::master)

        .def_property_readonly("image_size_in_bytes",
                               &CtbRawFile::image_size_in_bytes)

        .def_property_readonly("frames_in_file", &CtbRawFile::frames_in_file);

}