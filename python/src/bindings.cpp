#include <cstdint>
#include <filesystem>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include <string>

#include "aare/defs.hpp"
#include "aare/Frame.hpp"
#include "aare/FileHandler.hpp"
#include "aare/ImageData.hpp"
#include "aare/DataSpan.hpp"

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
        .def("get_array", &Frame<uint16_t>::get_array)
        .def_property_readonly("rows", &Frame<uint16_t>::rows)
        .def_property_readonly("cols", &Frame<uint16_t>::cols)
        .def_property_readonly("bitdepth", &Frame<uint16_t>::bitdepth);

    py::class_<DataSpan<uint16_t>>(m, "_DataSpan16")
        .def(py::init<std::byte*, ssize_t, ssize_t>())
        .def("get", &DataSpan<uint16_t>::get)
        .def("get_array", &DataSpan<uint16_t>::get_array)
        .def_property_readonly("rows", &DataSpan<uint16_t>::rows)
        .def_property_readonly("cols", &DataSpan<uint16_t>::cols)
        .def_property_readonly("bitdepth", &DataSpan<uint16_t>::bitdepth);

    py::class_<ImageData<uint16_t>>(m, "_ImageData16")
        .def(py::init<std::byte*, ssize_t, ssize_t>())
        .def("get", &ImageData<uint16_t>::get)
        .def("get_array", &ImageData<uint16_t>::get_array)
        .def_property_readonly("rows", &ImageData<uint16_t>::rows)
        .def_property_readonly("cols", &ImageData<uint16_t>::cols)
        .def_property_readonly("bitdepth", &ImageData<uint16_t>::bitdepth);

    



    

    
}


