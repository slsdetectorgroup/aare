#include <cstdint>
#include <filesystem>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include <string>

#include "aare/Frame.hpp"
#include "aare/IFrame.hpp"
#include "aare/defs.hpp"

#include "aare/DataSpan.hpp"
#include "aare/FileHandler.hpp"
#include "aare/ImageData.hpp"

namespace py = pybind11;

PYBIND11_MODULE(_aare, m) {
    // helps to convert from std::string to std::filesystem::path
    py::class_<std::filesystem::path>(m, "Path").def(py::init<std::string>());
    py::implicitly_convertible<std::string, std::filesystem::path>();

    // TODO: find a solution to avoid code duplication and include other detectors
    py::class_<FileHandler<DetectorType::Jungfrau, uint16_t>>(m, "_FileHandler_Jungfrau_16")
        .def(py::init<std::filesystem::path>())
        .def("get_frame", &FileHandler<DetectorType::Jungfrau, uint16_t>::get_frame);

    py::enum_<DetectorType>(m, "DetectorType");

    py::class_<IFrame<uint16_t>>(m, "_IFrame_16")
        .def("get", &IFrame<uint16_t>::get)
        .def("set", &IFrame<uint16_t>::set)
        .def("get_array", &IFrame<uint16_t>::get_array)
        .def_property_readonly("rows", &IFrame<uint16_t>::rows)
        .def_property_readonly("cols", &IFrame<uint16_t>::cols)
        .def_property_readonly("bitdepth", &IFrame<uint16_t>::bitdepth);

    py::class_<Frame<uint16_t>, IFrame<uint16_t>>(m, "_Frame_16")
        .def(py::init<std::byte *, ssize_t, ssize_t>());

    py::class_<DataSpan<uint16_t>, IFrame<uint16_t>>(m, "_DataSpan_16")
        .def(py::init<std::byte *, ssize_t, ssize_t>())
        .def(py::init<IFrame<uint16_t> &>());

    py::class_<ImageData<uint16_t>, IFrame<uint16_t>>(m, "_ImageData_16")
        .def(py::init<std::byte *, ssize_t, ssize_t>())
        .def(py::init<IFrame<uint16_t> &>());

}
