#include "aare/PixelMap.hpp"
#include "np_helper.hpp"


#include <cstdint>
#include <pybind11/numpy.h>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>


namespace py = pybind11;
using namespace::aare;


void define_pixel_map_bindings(py::module &m) {
    m.def("GenerateMoench03PixelMap", []() {
        auto ptr = new NDArray<ssize_t,2>(GenerateMoench03PixelMap());
        return return_image_data(ptr);
    })
    .def("GenerateMoench05PixelMap", []() {
        auto ptr = new NDArray<ssize_t,2>(GenerateMoench05PixelMap());
        return return_image_data(ptr);
    })
        .def("GenerateMoench05PixelMap1g", []() {
        auto ptr = new NDArray<ssize_t,2>(GenerateMoench05PixelMap1g());
        return return_image_data(ptr);
    })
    .def("GenerateMoench05PixelMapOld", []() {
        auto ptr = new NDArray<ssize_t,2>(GenerateMoench05PixelMapOld());
        return return_image_data(ptr);
    })
    .def("GenerateMH02SingleCounterPixelMap", []() {
        auto ptr = new NDArray<ssize_t,2>(GenerateMH02SingleCounterPixelMap());
        return return_image_data(ptr);
    })
    .def("GenerateMH02FourCounterPixelMap", []() {
        auto ptr = new NDArray<ssize_t,3>(GenerateMH02FourCounterPixelMap());
        return return_image_data(ptr);
    });

}