#include "aare/calibration.hpp"

#include <cstdint>
#include <filesystem>
#include <pybind11/numpy.h>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include <pybind11/stl_bind.h>

namespace py = pybind11;

template <typename DataType>
py::array_t<DataType> pybind_apply_calibration(
    py::array_t<uint16_t, py::array::c_style | py::array::forcecast> data,
    py::array_t<DataType, py::array::c_style | py::array::forcecast> pedestal,
    py::array_t<DataType, py::array::c_style | py::array::forcecast>
        calibration,
    int n_threads = 4) {
`
    auto data_span = make_view_3d(data);
    auto ped = make_view_3d(pedestal);
    auto cal = make_view_3d(calibration);

    /* No pointer is passed, so NumPy will allocate the buffer */
    auto result = py::array_t<DataType>(data_span.shape());
    auto res = make_view_3d(result);

    aare::apply_calibration<DataType>(res, data_span, ped, cal, n_threads);

    return result;
}

void bind_calibration(py::module &m) {
    m.def("apply_calibration", &pybind_apply_calibration<float>,
          py::arg("raw_data").noconvert(), py::kw_only(),
          py::arg("pd").noconvert(), py::arg("cal").noconvert(),
          py::arg("n_threads") = 4);

    m.def("apply_calibration", &pybind_apply_calibration<double>,
          py::arg("raw_data").noconvert(), py::kw_only(),
          py::arg("pd").noconvert(), py::arg("cal").noconvert(),
          py::arg("n_threads") = 4);
}