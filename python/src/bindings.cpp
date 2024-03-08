#include <cstdint>
#include <filesystem>
#include <pybind11/pybind11.h>
#include <string>

#include "aare/defs.hpp"
#include "aare/Frame.hpp"
#include "aare/FileHandler.hpp"

namespace py = pybind11;


PYBIND11_MODULE(_aare, m) {
    // helps to convert from std::string to std::filesystem::path
    py::class_<std::filesystem::path>(m, "Path")
        .def(py::init<std::string>());
        py::implicitly_convertible<std::string, std::filesystem::path>();

    //TODO: find a solution to avoid code duplication and include other detectors
    py::class_<FileHandler<DetectorType::Jungfrau, uint16_t>>(m, "_FileHandler_Jungfrau_16")
        .def(py::init<std::filesystem::path>())
        .def("get_frame", &FileHandler<DetectorType::Jungfrau, uint16_t>::get_frame);

    
    py::enum_<DetectorType>(m, "DetectorType");

    py::class_<Frame<uint16_t>>(m, "_Frame16")
        .def(py::init<std::byte*, ssize_t, ssize_t>())
        .def("get", &Frame<uint16_t>::get)
        .def_readonly("rows", &Frame<uint16_t>::rows)
        .def_readonly("cols", &Frame<uint16_t>::cols)
        .def_readonly("bitdepth", &Frame<uint16_t>::bitdepth);



    

    
}


