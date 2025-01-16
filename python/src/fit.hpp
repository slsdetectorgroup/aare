#include <cstdint>
#include <filesystem>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include <pybind11/stl_bind.h>

#include "aare/Fit.hpp"

namespace py = pybind11;

void define_fit_bindings(py::module &m) {
    m.def(
        "fit_gaus",
        [](py::array_t<double, py::array::c_style | py::array::forcecast> data,
           py::array_t<double, py::array::c_style | py::array::forcecast> x) {
            
            if(data.ndim() == 3){
                auto par = new NDArray<double, 3>{};
                auto data_view = make_view_3d(data);
                auto x_view = make_view_1d(x);
                *par = aare::fit_gaus(data_view, x_view);
                return return_image_data(par);
            }else if(data.ndim() == 1){
                auto par = new NDArray<double, 1>{};
                auto data_view = make_view_1d(data);
                auto x_view = make_view_1d(x);
                *par = aare::fit_gaus(data_view, x_view);
                return return_image_data(par);
            }else{
                throw std::runtime_error("Data must be 1D or 3D");
            }
            
        },
        py::arg("data"), py::arg("x"));
}