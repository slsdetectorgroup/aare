#include "aare/core/Frame.hpp"
#include "aare/core/defs.hpp"
#include "aare/file_io/File.hpp"
#include <cstdint>
#include <filesystem>
#include <pybind11/iostream.h>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include <string>

namespace py = pybind11;

void define_file_io_bindings(py::module &m) {

    py::class_<File>(m, "File")
        .def(py::init([](const std::filesystem::path &fname) { return File(fname, "r", {}); }))
        .def(py::init([](const std::filesystem::path &fname, const std::string &mode) {
            return File(fname, mode, {});
        }))
        .def(py::init<const std::filesystem::path &, const std::string &, const FileConfig &>())
        .def("read", py::overload_cast<>(&File::read))
        .def("read", py::overload_cast<size_t>(&File::read))
        .def("iread", py::overload_cast<size_t>(&File::iread))
        .def("frame_number", &File::frame_number)
        .def_property_readonly("bytes_per_frame", &File::bytes_per_frame)
        .def_property_readonly("pixels_per_frame", &File::pixels_per_frame)
        .def("seek", &File::seek)
        .def("tell", &File::tell)
        .def_property_readonly("total_frames", &File::total_frames)
        .def_property_readonly("rows", &File::rows)
        .def_property_readonly("cols", &File::cols)
        .def_property_readonly("bitdepth", &File::bitdepth)
        .def_property_readonly("detector_type", &File::detector_type)
        .def_property_readonly(
            "geometry", &File::geometry,
            py::call_guard<py::scoped_ostream_redirect, py::scoped_estream_redirect>())
        .def("set_total_frames", &File::set_total_frames);

    py::class_<FileConfig>(m, "FileConfig")
        .def(py::init<>())
        .def_readwrite("rows", &FileConfig::rows)
        .def_readwrite("cols", &FileConfig::cols)
        .def_readwrite("version", &FileConfig::version)
        .def_readwrite("geometry", &FileConfig::geometry)
        .def_readwrite("detector_type", &FileConfig::detector_type)
        .def_readwrite("max_frames_per_file", &FileConfig::max_frames_per_file)
        .def_readwrite("total_frames", &FileConfig::total_frames)
        .def_readwrite("dtype", &FileConfig::dtype)
        .def("__eq__", &FileConfig::operator==)
        .def("__ne__", &FileConfig::operator!=)
        .def("__repr__", [](const FileConfig &a) { return "<FileConfig: " + a.to_string() + ">"; });

}
