#include <catch2/catch_test_macros.hpp>
#include "aare/core/Dtype.hpp"
#include <pybind11/numpy.h>
#include <pybind11/pybind11.h>

namespace py = pybind11;
using aare::Dtype;
TEST_CASE("test dtype format_descr"){
    REQUIRE(Dtype(Dtype::UINT8).format_descr() == py::format_descriptor<uint8_t>::format());
    REQUIRE(Dtype(Dtype::UINT16).format_descr() == py::format_descriptor<uint16_t>::format());
    REQUIRE(Dtype(Dtype::UINT32).format_descr() == py::format_descriptor<uint32_t>::format());
    REQUIRE(Dtype(Dtype::UINT64).format_descr() == py::format_descriptor<uint64_t>::format());
    REQUIRE(Dtype(Dtype::INT8).format_descr() == py::format_descriptor<int8_t>::format());
    REQUIRE(Dtype(Dtype::INT16).format_descr() == py::format_descriptor<int16_t>::format());
    REQUIRE(Dtype(Dtype::INT32).format_descr() == py::format_descriptor<int32_t>::format());
    REQUIRE(Dtype(Dtype::INT64).format_descr() == py::format_descriptor<int64_t>::format());
    REQUIRE(Dtype(Dtype::FLOAT).format_descr() == py::format_descriptor<float>::format());
    REQUIRE(Dtype(Dtype::DOUBLE).format_descr() == py::format_descriptor<double>::format());

    

}