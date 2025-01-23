#include <cstdint>
#include <filesystem>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include <pybind11/stl_bind.h>

#include "aare/Fit.hpp"

namespace py = pybind11;

void define_fit_bindings(py::module &m) {

    //TODO! Evaluate without converting to double 
    m.def(
        "gaus",
        [](py::array_t<double, py::array::c_style | py::array::forcecast> x,
           py::array_t<double, py::array::c_style | py::array::forcecast> par) {
            auto x_view = make_view_1d(x);
            auto par_view = make_view_1d(par);
            auto y = new NDArray<double, 1>{aare::func::gauss(x_view, par_view)};
            return return_image_data(y);
        });

    m.def("affine",
          [](py::array_t<double, py::array::c_style | py::array::forcecast> x,
             py::array_t<double, py::array::c_style | py::array::forcecast> par) {
              auto x_view = make_view_1d(x);
              auto par_view = make_view_1d(par);
              auto y = new NDArray<double, 1>{aare::func::affine(x_view, par_view)};
              return return_image_data(y);
          });

    m.def(
        "fit_gaus",
        [](py::array_t<double, py::array::c_style | py::array::forcecast> x,
           py::array_t<double, py::array::c_style | py::array::forcecast> data) {
            if (data.ndim() == 3) {
                auto par = new NDArray<double, 3>{};
                auto data_view = make_view_3d(data);
                auto x_view = make_view_1d(x);
                *par = aare::fit_gaus(data_view, x_view);
                return return_image_data(par);
            } else if (data.ndim() == 1) {
                auto par = new NDArray<double, 1>{};
                auto data_view = make_view_1d(data);
                auto x_view = make_view_1d(x);
                *par = aare::fit_gaus(data_view, x_view);
                return return_image_data(par);
            } else {
                throw std::runtime_error("Data must be 1D or 3D");
            }
        },
        py::arg("data"), py::arg("x"));

    m.def(
        "fit_gaus",
        [](py::array_t<double, py::array::c_style | py::array::forcecast> x,
           py::array_t<double, py::array::c_style | py::array::forcecast> y,
           py::array_t<double, py::array::c_style | py::array::forcecast>
               y_err) {
            if (y.ndim() == 3) {
                //Allocate memory for the output
                //Need to have pointers to allow python to manage 
                //the memory
                auto par = new NDArray<double, 3>({y.shape(0), y.shape(1),3});
                auto par_err = new NDArray<double, 3>({y.shape(0), y.shape(1),3});

                auto y_view = make_view_3d(y);
                auto y_view_err = make_view_3d(y_err);
                auto x_view = make_view_1d(x);
                aare::fit_gaus(x_view,y_view, y_view_err, par->view(), par_err->view());
                // return return_image_data(par);
                return py::make_tuple(return_image_data(par), return_image_data(par_err));
            } else if (y.ndim() == 1) {
                //Allocate memory for the output
                //Need to have pointers to allow python to manage 
                //the memory
                auto par = new NDArray<double, 1>({3});
                auto par_err = new NDArray<double, 1>({3});

                //Decode the numpy arrays
                auto y_view = make_view_1d(y);
                auto y_view_err = make_view_1d(y_err);
                auto x_view = make_view_1d(x);

                aare::fit_gaus(x_view, y_view, y_view_err, par->view(), par_err->view());
                return py::make_tuple(return_image_data(par), return_image_data(par_err));
            } else {
                throw std::runtime_error("Data must be 1D or 3D");
            }
        },
        py::arg("data"), py::arg("x"), py::arg("data_err"));
    

        m.def(
        "fit_affine",
        [](py::array_t<double, py::array::c_style | py::array::forcecast> x,
           py::array_t<double, py::array::c_style | py::array::forcecast> data,
           py::array_t<double, py::array::c_style | py::array::forcecast>
               data_err) {
            if (data.ndim() == 3) {
                auto par = new NDArray<double, 3>({data.shape(0), data.shape(1),2});
                auto par_err = new NDArray<double, 3>({data.shape(0), data.shape(1),2});

                auto data_view = make_view_3d(data);
                auto data_view_err = make_view_3d(data_err);
                auto x_view = make_view_1d(x);

                aare::fit_affine(x_view, data_view, data_view_err, par->view(), par_err->view());
                return py::make_tuple(return_image_data(par), return_image_data(par_err));
 
            } else if (data.ndim() == 1) {
                auto par = new NDArray<double, 1>({2});
                auto par_err = new NDArray<double, 1>({2});

                auto data_view = make_view_1d(data);
                auto data_view_err = make_view_1d(data_err);
                auto x_view = make_view_1d(x);

                aare::fit_affine(x_view, data_view, data_view_err, par->view(), par_err->view());
                return py::make_tuple(return_image_data(par), return_image_data(par_err));
            } else {
                throw std::runtime_error("Data must be 1D or 3D");
            }
        },
        py::arg("data"), py::arg("x"), py::arg("data_err"));
}