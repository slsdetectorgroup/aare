#include <cstdint>
#include <filesystem>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include <string>

#include "aare/core/Frame.hpp"
#include "aare/core/defs.hpp"
#include "aare/file_io/File.hpp"

#include "NDArray_bindings.hpp"
#include "NDView_bindings.hpp"

namespace py = pybind11;

PYBIND11_MODULE(_aare, m) {
    // helps to convert from std::string to std::filesystem::path
    py::class_<std::filesystem::path>(m, "Path").def(py::init<std::string>());
    py::implicitly_convertible<std::string, std::filesystem::path>();

    py::class_<Frame>(m, "Frame")
        .def(py::init<std::byte *, ssize_t, ssize_t, ssize_t>())
        .def_property_readonly("rows", &Frame::rows)
        .def_property_readonly("cols", &Frame::cols)
        .def_property_readonly("bitdepth", &Frame::bitdepth)
        .def_property_readonly("size", &Frame::size)
        .def_property_readonly("data", &Frame::data, py::return_value_policy::reference)
        .def("p",[&](Frame &f) -> void  {
            py::print("int8", DType(DType::TypeIndex::INT8).format_descr()," ", py::format_descriptor<int8_t>::format());
            py::print("uint8", DType(DType::TypeIndex::UINT8).format_descr()," ", py::format_descriptor<uint8_t>::format());
            py::print("int16", DType(DType::TypeIndex::INT16).format_descr()," ", py::format_descriptor<int16_t>::format());
            py::print("uint16", DType(DType::TypeIndex::UINT16).format_descr()," ", py::format_descriptor<uint16_t>::format());
            py::print("int32", DType(DType::TypeIndex::INT32).format_descr()," ", py::format_descriptor<int32_t>::format());
            py::print("uint32", DType(DType::TypeIndex::UINT32).format_descr()," ", py::format_descriptor<uint32_t>::format());
            py::print("int64", DType(DType::TypeIndex::INT64).format_descr()," ", py::format_descriptor<int64_t>::format());
            py::print("uint64", DType(DType::TypeIndex::UINT64).format_descr()," ", py::format_descriptor<uint64_t>::format());
            py::print("float", DType(DType::TypeIndex::FLOAT).format_descr()," ", py::format_descriptor<float>::format());
            py::print("double", DType(DType::TypeIndex::DOUBLE).format_descr()," ", py::format_descriptor<double>::format());

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

    py::enum_<DType::TypeIndex>(m, "DTypeIndex")
        .value("INT8", DType::TypeIndex::INT8)
        .value("UINT8", DType::TypeIndex::UINT8)
        .value("INT16", DType::TypeIndex::INT16)
        .value("UINT16", DType::TypeIndex::UINT16)
        .value("INT32", DType::TypeIndex::INT32)
        .value("UINT32", DType::TypeIndex::UINT32)
        .value("INT64", DType::TypeIndex::INT64)
        .value("UINT64", DType::TypeIndex::UINT64)
        .value("FLOAT", DType::TypeIndex::FLOAT)
        .value("DOUBLE", DType::TypeIndex::DOUBLE)
        .value("ERROR", DType::TypeIndex::ERROR)
        .export_values();

    py::class_<DType>(m, "DType")
        .def(py::init<std::string_view>())
        .def(py::init<DType::TypeIndex>())
        .def("bitdepth", &DType::bitdepth)
        .def("bytes", &DType::bytes)
        .def("to_string", &DType::to_string);

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
        .def("__repr__", &FileConfig::to_string);


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

    py::class_<File>(m, "File")
        .def(py::init([](const std::filesystem::path &fname) { return File(fname, "r", {}); }))
        .def(
            py::init([](const std::filesystem::path &fname, const std::string &mode) { return File(fname, mode, {}); }))
        .def(py::init<const std::filesystem::path &, const std::string &, const FileConfig &>())
        .def("write", &File::write)
        .def("read", py::overload_cast<>(&File::read))
        .def("read", py::overload_cast<uint64_t>(&File::read))
        .def("iread", py::overload_cast<size_t>(&File::iread))
        .def("frame_number", &File::frame_number)
        .def("bytes_per_frame", &File::bytes_per_frame)
        .def("pixels_per_frame", &File::pixels_per_frame)
        .def("seek", &File::seek)
        .def("tell", &File::tell)
        .def("total_frames", &File::total_frames)
        .def("rows", &File::rows)
        .def("cols", &File::cols)
        .def("bitdepth", &File::bitdepth)
        .def("set_total_frames", &File::set_total_frames);
        

    define_NDArray_bindings<double, 2>(m);
    define_NDArray_bindings<float, 2>(m);
    define_NDArray_bindings<int, 2>(m);
    define_NDArray_bindings<unsigned int, 2>(m);
    define_NDArray_bindings<uint64_t, 2>(m);

    define_NDView_bindings<double, 2>(m);
    define_NDView_bindings<float, 2>(m);
    define_NDView_bindings<int, 2>(m);
    define_NDView_bindings<unsigned int, 2>(m);
    define_NDView_bindings<uint64_t, 2>(m);
}
