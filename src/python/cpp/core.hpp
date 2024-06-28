
#include <cstdint>
#include <filesystem>
#include <pybind11/functional.h>
#include <pybind11/numpy.h>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include <string>

#include "aare/core/Frame.hpp"
#include "aare/core/Transforms.hpp"
#include "aare/core/defs.hpp"

template <typename T> void define_to_frame(py::module &m) {
    m.def("to_frame", [](py::array_t<T> &np_array) {
        py::buffer_info info = np_array.request();
        if (info.format != py::format_descriptor<T>::format())
            throw std::runtime_error(
                "Incompatible format: different formats! (Are you sure the arrays are of the same type?)");
        if (info.ndim != 2)
            throw std::runtime_error("Incompatible dimension: expected a 2D array!");

        Frame f(info.shape[0], info.shape[1], Dtype(typeid(T)));
        std::memcpy(f.data(), info.ptr, f.bytes());
        return f;
    });
}
void define_core_bindings(py::module &m) {
    py::class_<Frame>(m, "Frame", py::buffer_protocol())
        .def(py::init<std::byte *, int64_t, int64_t, Dtype>())
        .def(py::init<int64_t, int64_t, Dtype>())
        .def_property_readonly("rows", &Frame::rows)
        .def_property_readonly("cols", &Frame::cols)
        .def_property_readonly("bitdepth", &Frame::bitdepth)
        .def_property_readonly("size", &Frame::bytes)
        .def_property_readonly("data", &Frame::data, py::return_value_policy::reference)
        .def_buffer([](Frame &f) -> py::buffer_info {
            Dtype dt = f.dtype();
            return {
                f.data(),                           /* Pointer to buffer */
                static_cast<int64_t>(dt.bytes()),   /* Size of one scalar */
                dt.format_descr(),                  /* Python struct-style format descriptor */
                2,                                  /* Number of dimensions */
                {f.rows(), f.cols()},               /* Buffer dimensions */
                {f.cols() * dt.bytes(), dt.bytes()} /* Strides (in bytes) for each index */
            };
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

    py::enum_<Dtype::TypeIndex>(m, "DtypeIndex")
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

    py::class_<Dtype>(m, "Dtype")
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

    py::class_<Transforms>(m, "Transforms")
        .def(py::init<>())
        .def_static("identity", &Transforms::identity)
        .def_static("zero", &Transforms::zero)
        .def_static("reorder", py::overload_cast<NDView<size_t, 2> &>(&Transforms::reorder))
        .def_static("reorder", py::overload_cast<NDArray<size_t, 2> &>(&Transforms::reorder))
        .def_static("reorder", py::overload_cast<std::vector<size_t> &>(&Transforms::reorder))
        .def_static(
            // only accept order_map of type uint64_t
            "reorder",
            [](py::array_t<uint64_t> &np_array) {
                py::buffer_info info = np_array.request();
                if (info.format != Dtype(Dtype::UINT64).format_descr())
                    throw std::runtime_error(
                        "Incompatible format: different formats! (Are you sure the arrays are of the same type?)");
                if (info.ndim != 2)
                    throw std::runtime_error("Incompatible dimension: expected a 2D array!");

                std::array<int64_t, 2> arr_shape;
                std::move(info.shape.begin(), info.shape.end(), arr_shape.begin());

                NDView<uint64_t, 2> a(static_cast<uint64_t *>(info.ptr), arr_shape);
                return Transforms::reorder(a);
            })
        .def_static("flip_horizental", &Transforms::flip_horizental)
        .def("add", [](Transforms &self, std::function<Frame &(Frame &)> transformation) { self.add(transformation); })
        .def("add", [](Transforms &self,
                       std::vector<std::function<Frame &(Frame &)>> transformations) { self.add(transformations); })
        .def("__call__", &Transforms::apply);

    define_to_frame<uint8_t>(m);
    define_to_frame<uint16_t>(m);
    define_to_frame<uint32_t>(m);
    define_to_frame<uint64_t>(m);
    define_to_frame<int8_t>(m);
    define_to_frame<int16_t>(m);
    define_to_frame<int32_t>(m);
    define_to_frame<int64_t>(m);
    define_to_frame<float>(m);
    define_to_frame<double>(m);
}