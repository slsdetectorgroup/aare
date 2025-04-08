
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

// Disable warnings for unused parameters, as we ignore some
// in the __exit__ method
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"

auto read_dat_frame(JungfrauDataFile &self) {
    py::array_t<JungfrauDataHeader> header(1);
    py::array_t<uint16_t> image({
        self.rows(), 
        self.cols() 
    });

    self.read_into(reinterpret_cast<std::byte *>(image.mutable_data()),
                   header.mutable_data());

    return py::make_tuple(header, image);
}

auto read_n_dat_frames(JungfrauDataFile &self, size_t n_frames) {
    // adjust for actual frames left in the file
    n_frames = std::min(n_frames, self.total_frames() - self.tell());
    if (n_frames == 0) {
        throw std::runtime_error("No frames left in file");
    }

    py::array_t<JungfrauDataHeader> header(n_frames);
    py::array_t<uint16_t> image({
        n_frames, self.rows(), 
        self.cols()});

    self.read_into(reinterpret_cast<std::byte *>(image.mutable_data()),
                   n_frames, header.mutable_data());

    return py::make_tuple(header, image);
}

void define_jungfrau_data_file_io_bindings(py::module &m) {
    // Make the JungfrauDataHeader usable from numpy
    PYBIND11_NUMPY_DTYPE(JungfrauDataHeader, framenum, bunchid);

    py::class_<JungfrauDataFile>(m, "JungfrauDataFile")
        .def(py::init<const std::filesystem::path &>())
        .def("seek", &JungfrauDataFile::seek,
             R"(
               Seek to the given frame index.
               )")
        .def("tell", &JungfrauDataFile::tell,
             R"(
               Get the current frame index.
               )")
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
        .def_property_readonly("current_file", &JungfrauDataFile::current_file)
        .def_property_readonly("total_frames", &JungfrauDataFile::total_frames)
        .def_property_readonly("n_files", &JungfrauDataFile::n_files)
        .def("read_frame", &read_dat_frame,
             R"(
               Read a single frame from the file.
               )")
        .def("read_n", &read_n_dat_frames,
             R"(
               Read maximum n_frames frames from the file.
               )")
        .def(
            "read",
            [](JungfrauDataFile &self) {
                self.seek(0);
                auto n_frames = self.total_frames();
                return read_n_dat_frames(self, n_frames);
            },
            R"(
              Read all frames from the file. Seeks to the beginning before reading.
              )")
        .def("__enter__", [](JungfrauDataFile &self) { return &self; })
        .def("__exit__",
             [](JungfrauDataFile &self,
                const std::optional<pybind11::type> &exc_type,
                const std::optional<pybind11::object> &exc_value,
                const std::optional<pybind11::object> &traceback) {
                 //  self.close();
             })
        .def("__iter__", [](JungfrauDataFile &self) { return &self; })
        .def("__next__", [](JungfrauDataFile &self) {
            try {
                return read_dat_frame(self);
            } catch (std::runtime_error &e) {
                throw py::stop_iteration();
            }
        });
}

#pragma GCC diagnostic pop