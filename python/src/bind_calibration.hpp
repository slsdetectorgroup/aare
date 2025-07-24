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

    auto data_span = make_view_3d(data); // data is always 3D
    /* No pointer is passed, so NumPy will allocate the buffer */
    auto result = py::array_t<DataType>(data_span.shape());
    auto res = make_view_3d(result);
    if (data.ndim() == 3 && pedestal.ndim() == 3 && calibration.ndim() == 3) {
        auto ped = make_view_3d(pedestal);
        auto cal = make_view_3d(calibration);
        aare::apply_calibration<DataType, 3>(res, data_span, ped, cal,
                                             n_threads);
    } else if (data.ndim() == 3 && pedestal.ndim() == 2 &&
               calibration.ndim() == 2) {
        auto ped = make_view_2d(pedestal);
        auto cal = make_view_2d(calibration);
        aare::apply_calibration<DataType, 2>(res, data_span, ped, cal,
                                             n_threads);
    } else {
        throw std::runtime_error(
            "Invalid number of dimensions for data, pedestal or calibration");
    }
    return result;
}

py::array_t<int> pybind_count_switching_pixels(
    py::array_t<uint16_t, py::array::c_style | py::array::forcecast> data,
    ssize_t n_threads = 4) {

    auto data_span = make_view_3d(data);
    auto arr = new NDArray<int, 2>{};
    *arr = aare::count_switching_pixels(data_span, n_threads);
    return return_image_data(arr);
}

template <typename T>
py::array_t<T> pybind_calculate_pedestal(
    py::array_t<uint16_t, py::array::c_style | py::array::forcecast> data,
    ssize_t n_threads) {

    auto data_span = make_view_3d(data);
    auto arr = new NDArray<T, 3>{};
    *arr = aare::calculate_pedestal<T, false>(data_span, n_threads);
    return return_image_data(arr);
}

template <typename T>
py::array_t<T> pybind_calculate_pedestal_g0(
    py::array_t<uint16_t, py::array::c_style | py::array::forcecast> data,
    ssize_t n_threads) {

    auto data_span = make_view_3d(data);
    auto arr = new NDArray<T, 2>{};
    *arr = aare::calculate_pedestal<T, true>(data_span, n_threads);
    return return_image_data(arr);
}

void bind_calibration(py::module &m) {
    m.def("apply_calibration", &pybind_apply_calibration<double>,
          py::arg("raw_data").noconvert(), py::kw_only(),
          py::arg("pd").noconvert(), py::arg("cal").noconvert(),
          py::arg("n_threads") = 4);

    m.def("apply_calibration", &pybind_apply_calibration<float>,
          py::arg("raw_data").noconvert(), py::kw_only(),
          py::arg("pd").noconvert(), py::arg("cal").noconvert(),
          py::arg("n_threads") = 4);

    m.def("count_switching_pixels", &pybind_count_switching_pixels,
          R"(
        Count the number of time each pixel switches to G1 or G2.

        Parameters
        ----------
        raw_data : array_like
            3D array of shape (frames, rows, cols) to count the switching pixels from.
        n_threads : int 
            The number of threads to use for the calculation.
        )",
          py::arg("raw_data").noconvert(), py::kw_only(),
          py::arg("n_threads") = 4);

    m.def("calculate_pedestal", &pybind_calculate_pedestal<double>,
          R"(
        Calculate the pedestal for all three gains and return the result as a 3D array of doubles.

        Parameters
        ----------
        raw_data : array_like
            3D array of shape (frames, rows, cols) to calculate the pedestal from.
            Needs to contain data for all three gains (G0, G1, G2).
        n_threads : int 
            The number of threads to use for the calculation.
        )",
          py::arg("raw_data").noconvert(), py::arg("n_threads") = 4);

    m.def("calculate_pedestal_float", &pybind_calculate_pedestal<float>,
          R"(
        Same as `calculate_pedestal` but returns a 3D array of floats.

        Parameters
        ----------
        raw_data : array_like
            3D array of shape (frames, rows, cols) to calculate the pedestal from.
            Needs to contain data for all three gains (G0, G1, G2).
        n_threads : int 
            The number of threads to use for the calculation.
        )",
          py::arg("raw_data").noconvert(), py::arg("n_threads") = 4);

    m.def("calculate_pedestal_g0", &pybind_calculate_pedestal_g0<double>,
          R"(
        Calculate the pedestal for G0 and return the result as a 2D array of doubles.
        Pixels in G1 and G2 are ignored.

        Parameters
        ----------
        raw_data : array_like
            3D array of shape (frames, rows, cols) to calculate the pedestal from.
        n_threads : int 
            The number of threads to use for the calculation.
        )",
          py::arg("raw_data").noconvert(), py::arg("n_threads") = 4);

    m.def("calculate_pedestal_g0_float", &pybind_calculate_pedestal_g0<float>,
          R"(
        Same as `calculate_pedestal_g0` but returns a 2D array of floats.

        Parameters
        ----------
        raw_data : array_like
            3D array of shape (frames, rows, cols) to calculate the pedestal from.
        n_threads : int 
            The number of threads to use for the calculation.
        )",
          py::arg("raw_data").noconvert(), py::arg("n_threads") = 4);
}