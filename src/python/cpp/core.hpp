
#include <cstdint>
#include <filesystem>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include <string>

#include "aare/core/Frame.hpp"
#include "aare/core/defs.hpp"
#include "aare/file_io/File.hpp"

void define_core_bindings(py::module &m) {
    py::class_<Frame>(m, "Frame")
        .def(py::init<std::byte *, int64_t, int64_t, int64_t>())
        .def_property_readonly("rows", &Frame::rows)
        .def_property_readonly("cols", &Frame::cols)
        .def_property_readonly("bitdepth", &Frame::bitdepth)
        .def_property_readonly("size", &Frame::size)
        .def_property_readonly("data", &Frame::data, py::return_value_policy::reference)
        // TODO: add DType to Frame so that we can define def_buffer()
        // we can use format_descr() to get the format descriptor
        .def("array", [&](Frame &f) -> py::array {
            py::array arr;
            if (f.bitdepth() == 8) {
                arr = py::array_t<uint8_t>({f.rows(), f.cols()});
            } else if (f.bitdepth() == 16) {
                arr = py::array_t<uint16_t>({f.rows(), f.cols()});
            } else if (f.bitdepth() == 32) {
                arr = py::array_t<uint32_t>({f.rows(), f.cols()});
            } else if (f.bitdepth() == 64) {
                arr = py::array_t<uint64_t>({f.rows(), f.cols()});
            } else {
                throw std::runtime_error("Unsupported bitdepth");
            }

            std::memcpy(arr.mutable_data(), f.data(), f.size());
            return arr;
        });

    py::class_<xy>(m, "xy")
        .def(py::init<>())
        .def(py::init<uint32_t, uint32_t>())
        .def_readwrite("row", &xy::row)
        .def_readwrite("col", &xy::col)
        .def("__eq__", &xy::operator==)
        .def("__ne__", &xy::operator!=)
        .def("__repr__",
             [](const xy &a) { return "<xy: row=" + std::to_string(a.row) + ", col=" + std::to_string(a.col) + ">"; });

    py::enum_<DetectorType>(m, "DetectorType")
        .value("Jungfrau", DetectorType::Jungfrau)
        .value("Eiger", DetectorType::Eiger)
        .value("Mythen3", DetectorType::Mythen3)
        .value("Moench", DetectorType::Moench)
        .value("ChipTestBoard", DetectorType::ChipTestBoard)
        .value("Unknown", DetectorType::Unknown)
        .export_values();

    py::enum_<TimingMode>(m, "TimingMode")
        .value("Auto", TimingMode::Auto)
        .value("Trigger", TimingMode::Trigger)
        .export_values();

    py::enum_<endian>(m, "endian")
        .value("big", endian::big)
        .value("little", endian::little)
        .value("native", endian::native)
        .export_values();

    py::enum_<Dtype::TypeIndex>(m, "DTypeIndex")
        .value("INT8", Dtype::TypeIndex::INT8)
        .value("UINT8", Dtype::TypeIndex::UINT8)
        .value("INT16", Dtype::TypeIndex::INT16)
        .value("UINT16", Dtype::TypeIndex::UINT16)
        .value("INT32", Dtype::TypeIndex::INT32)
        .value("UINT32", Dtype::TypeIndex::UINT32)
        .value("INT64", Dtype::TypeIndex::INT64)
        .value("UINT64", Dtype::TypeIndex::UINT64)
        .value("FLOAT", Dtype::TypeIndex::FLOAT)
        .value("DOUBLE", Dtype::TypeIndex::DOUBLE)
        .value("ERROR", Dtype::TypeIndex::ERROR)
        .export_values();

    py::class_<Dtype>(m, "DType")
        .def(py::init<std::string_view>())
        .def(py::init<Dtype::TypeIndex>())
        .def("bitdepth", &Dtype::bitdepth)
        .def("bytes", &Dtype::bytes)
        .def("to_string", &Dtype::to_string);

    py::class_<sls_detector_header>(m, "sls_detector_header")
        .def(py::init<>())
        .def_readwrite("frameNumber", &sls_detector_header::frameNumber)
        .def_readwrite("expLength", &sls_detector_header::expLength)
        .def_readwrite("packetNumber", &sls_detector_header::packetNumber)
        .def_readwrite("bunchId", &sls_detector_header::bunchId)
        .def_readwrite("timestamp", &sls_detector_header::timestamp)
        .def_readwrite("modId", &sls_detector_header::modId)
        .def_readwrite("row", &sls_detector_header::row)
        .def_readwrite("column", &sls_detector_header::column)
        .def_readwrite("reserved", &sls_detector_header::reserved)
        .def_readwrite("debug", &sls_detector_header::debug)
        .def_readwrite("roundRNumber", &sls_detector_header::roundRNumber)
        .def_readwrite("detType", &sls_detector_header::detType)
        .def_readwrite("version", &sls_detector_header::version)
        .def_readwrite("packetMask", &sls_detector_header::packetMask)
        .def("__repr__", &sls_detector_header::to_string);
}