#include <cstdint>
#include <filesystem>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include <string>

#include "aare/FileHandler.hpp"
#include "aare/Frame.hpp"
#include "aare/defs.hpp"

namespace py = pybind11;

PYBIND11_MODULE(_aare, m) {
    // helps to convert from std::string to std::filesystem::path
    py::class_<std::filesystem::path>(m, "Path").def(py::init<std::string>());
    py::implicitly_convertible<std::string, std::filesystem::path>();

    // TODO: find a solution to avoid code duplication and include other detectors
    py::class_<FileHandler>(m, "_FileHandler")
        .def(py::init<std::filesystem::path>())
        .def("get_frame", &FileHandler::get_frame);

    py::enum_<DetectorType>(m, "DetectorType");

    py::class_<Frame>(m, "_Frame")
        .def(py::init<std::byte *, ssize_t, ssize_t, ssize_t>())
        .def("get", &Frame::get)
        .def_property_readonly("rows", &Frame::rows)
        .def_property_readonly("cols", &Frame::cols)
        .def_property_readonly("bitdepth", &Frame::bitdepth);
}
