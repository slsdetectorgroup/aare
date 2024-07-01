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
#include "core.hpp"
#include "file_io.hpp"
#include "processing.hpp"

namespace py = pybind11;

PYBIND11_MODULE(_aare, m) {
    // helps to convert from std::string to std::filesystem::path
    py::class_<std::filesystem::path>(m, "Path").def(py::init<std::string>());
    py::implicitly_convertible<std::string, std::filesystem::path>();

    define_core_bindings(m);
    define_file_io_bindings(m);
    define_processing_bindings(m);

    define_NDArray_bindings<double, 2>(m);
    define_NDArray_bindings<float, 2>(m);
    define_NDArray_bindings<uint8_t, 2>(m);
    define_NDArray_bindings<uint16_t, 2>(m);
    define_NDArray_bindings<uint32_t, 2>(m);
    define_NDArray_bindings<uint64_t, 2>(m);
    define_NDArray_bindings<int8_t, 2>(m);
    define_NDArray_bindings<int16_t, 2>(m);
    define_NDArray_bindings<int32_t, 2>(m);
    define_NDArray_bindings<int64_t, 2>(m);

    define_NDView_bindings<double, 2>(m);
    define_NDView_bindings<float, 2>(m);
    define_NDView_bindings<uint8_t, 2>(m);
    define_NDView_bindings<uint16_t, 2>(m);
    define_NDView_bindings<uint32_t, 2>(m);
    define_NDView_bindings<uint64_t, 2>(m);
    define_NDView_bindings<int8_t, 2>(m);
    define_NDView_bindings<int16_t, 2>(m);
    define_NDView_bindings<int32_t, 2>(m);
    define_NDView_bindings<int64_t, 2>(m);
}
