
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
    //Make the JungfrauDataHeader usable from numpy
    PYBIND11_NUMPY_DTYPE(JungfrauDataHeader, framenum, bunchid);

    py::class_<JungfrauDataFile>(m, "JungfrauDataFile")
        .def(py::init<const std::filesystem::path &>())
        .def("seek", &JungfrauDataFile::seek)
        .def("tell", &JungfrauDataFile::tell)
        .def_property_readonly("rows", &JungfrauDataFile::rows)
        .def_property_readonly("cols", &JungfrauDataFile::cols)
        .def_property_readonly("base_name", &JungfrauDataFile::base_name)
        .def_property_readonly("bytes_per_frame",
                               &JungfrauDataFile::bytes_per_frame)
        .def_property_readonly("pixels_per_frame",
                               &JungfrauDataFile::pixels_per_frame)
        .def_property_readonly("bytes_per_pixel",
                               &JungfrauDataFile::bytes_per_pixel)
        .def_property_readonly("bitdepth", &JungfrauDataFile::bitdepth)
        .def_property_readonly("current_file",
                               &JungfrauDataFile::current_file)
        .def_property_readonly("total_frames",
                               &JungfrauDataFile::total_frames)
        .def_property_readonly("n_files", &JungfrauDataFile::n_files)
        .def("read_frame",
            [](JungfrauDataFile &self) {
                
                std::vector<ssize_t> shape;
                shape.reserve(2);
                shape.push_back(self.rows());
                shape.push_back(self.cols());

                // return headers from all subfiles
                py::array_t<JungfrauDataHeader> header(1);
                py::array_t<uint16_t> image(shape);

                self.read_into(
                    reinterpret_cast<std::byte *>(image.mutable_data()),
                    header.mutable_data());

                return py::make_tuple(header, image);
            })
        .def("read_n",
            [](JungfrauDataFile &self, size_t n_frames) {
                // adjust for actual frames left in the file
                n_frames =
                    std::min(n_frames, self.total_frames() - self.tell());
                if (n_frames == 0) {
                    throw std::runtime_error("No frames left in file");
                }
                std::vector<size_t> shape{n_frames, self.rows(), self.cols()};

                // return headers from all subfiles
                py::array_t<JungfrauDataHeader> header(n_frames);

                py::array_t<uint16_t> image(shape);

                self.read_into(
                    reinterpret_cast<std::byte *>(image.mutable_data()),
                    n_frames,
                    header.mutable_data());

                return py::make_tuple(header, image);
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