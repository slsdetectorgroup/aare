
#include "aare/JungfrauDataFile.hpp"
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

void define_jungfrau_data_file_io_bindings(py::module &m) {
    py::class_<JungfrauDataFile>(m, "JungfrauDataFile")
        .def(py::init<const std::filesystem::path &>())
        .def("guess_frame_size",
             [](const std::filesystem::path &fname) {
                 return JungfrauDataFile::guess_frame_size(fname);
             })
        .def_property_readonly("rows", &JungfrauDataFile::rows)
        .def_property_readonly("cols", &JungfrauDataFile::cols)
        .def_property_readonly("base_name", &JungfrauDataFile::base_name)
        .def("get_frame_path",
             [](const std::filesystem::path &path, const std::string &base_name,
                size_t frame_index) {
                 return JungfrauDataFile::get_frame_path(path, base_name,
                                                        frame_index);
             });
        // .def("read_frame",
        //      [](RawFile &self) {
        //          py::array image;
        //          std::vector<ssize_t> shape;
        //          shape.reserve(2);
        //          shape.push_back(self.rows());
        //          shape.push_back(self.cols());

        //          // return headers from all subfiles
        //          py::array_t<DetectorHeader> header(self.n_mod());

        //          const uint8_t item_size = self.bytes_per_pixel();
        //          if (item_size == 1) {
        //              image = py::array_t<uint8_t>(shape);
        //          } else if (item_size == 2) {
        //              image = py::array_t<uint16_t>(shape);
        //          } else if (item_size == 4) {
        //              image = py::array_t<uint32_t>(shape);
        //          }
        //          self.read_into(
        //              reinterpret_cast<std::byte *>(image.mutable_data()),
        //              header.mutable_data());

        //          return py::make_tuple(header, image);
        //      })

}