#include <cstdint>
#include <filesystem>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include <pybind11/stl_bind.h>

#include "aare/Fit.hpp"

namespace py = pybind11;
using namespace pybind11::literals;

void define_fit_bindings(py::module &m) {

    // TODO! Evaluate without converting to double
    m.def(
        "gaus",
        [](py::array_t<double, py::array::c_style | py::array::forcecast> x,
           py::array_t<double, py::array::c_style | py::array::forcecast> par) {
            auto x_view = make_view_1d(x);
            auto par_view = make_view_1d(par);
            auto y = new NDArray<double, 1>{aare::func::gaus(x_view, par_view)};
            return return_image_data(y);
        },
        R"(
        Evaluate a 1D Gaussian function for all points in x using parameters par.

        Parameters
        ----------
        x : array_like
            The points at which to evaluate the Gaussian function.
        par : array_like
            The parameters of the Gaussian function. The first element is the amplitude, the second element is the mean, and the third element is the standard deviation.
        )",
        py::arg("x"), py::arg("par"));

    m.def(
        "pol1",
        [](py::array_t<double, py::array::c_style | py::array::forcecast> x,
           py::array_t<double, py::array::c_style | py::array::forcecast> par) {
            auto x_view = make_view_1d(x);
            auto par_view = make_view_1d(par);
            auto y = new NDArray<double, 1>{aare::func::pol1(x_view, par_view)};
            return return_image_data(y);
        },
        R"(
        Evaluate a 1D polynomial function for all points in x using parameters par. (p0+p1*x)
        
        Parameters
        ----------
        x : array_like
            The points at which to evaluate the polynomial function.
        par : array_like
            The parameters of the polynomial function. The first element is the intercept, and the second element is the slope.    
        )",
        py::arg("x"), py::arg("par"));

    m.def(
        "scurve",
        [](py::array_t<double, py::array::c_style | py::array::forcecast> x,
           py::array_t<double, py::array::c_style | py::array::forcecast> par) {
            auto x_view = make_view_1d(x);
            auto par_view = make_view_1d(par);
            auto y =
                new NDArray<double, 1>{aare::func::scurve(x_view, par_view)};
            return return_image_data(y);
        },
        R"(
        Evaluate a 1D scurve function for all points in x using parameters par. 
        
        Parameters
        ----------
        x : array_like
            The points at which to evaluate the scurve function.
        par : array_like
            The parameters of the scurve function. The first element is the background slope, the second element is the background intercept, the third element is the mean, the fourth element is the standard deviation, the fifth element is inflexion point count number, and the sixth element is C.     
        )",
        py::arg("x"), py::arg("par"));

    m.def(
        "scurve2",
        [](py::array_t<double, py::array::c_style | py::array::forcecast> x,
           py::array_t<double, py::array::c_style | py::array::forcecast> par) {
            auto x_view = make_view_1d(x);
            auto par_view = make_view_1d(par);
            auto y =
                new NDArray<double, 1>{aare::func::scurve2(x_view, par_view)};
            return return_image_data(y);
        },
        R"(
        Evaluate a 1D scurve2 function for all points in x using parameters par. 
        
        Parameters
        ----------
        x : array_like
            The points at which to evaluate the scurve function.
        par : array_like
            The parameters of the scurve2 function. The first element is the background slope, the second element is the background intercept, the third element is the mean, the fourth element is the standard deviation, the fifth element is inflexion point count number, and the sixth element is C.     
        )",
        py::arg("x"), py::arg("par"));

    m.def(
        "fit_gaus",
        [](py::array_t<double, py::array::c_style | py::array::forcecast> x,
           py::array_t<double, py::array::c_style | py::array::forcecast> y,
           int n_threads) {
            if (y.ndim() == 3) {
                auto par = new NDArray<double, 3>{};
                auto y_view = make_view_3d(y);
                auto x_view = make_view_1d(x);
                *par = aare::fit_gaus(x_view, y_view, n_threads);
                return return_image_data(par);
            } else if (y.ndim() == 1) {
                auto par = new NDArray<double, 1>{};
                auto y_view = make_view_1d(y);
                auto x_view = make_view_1d(x);
                *par = aare::fit_gaus(x_view, y_view);
                return return_image_data(par);
            } else {
                throw std::runtime_error("Data must be 1D or 3D");
            }
        },
        R"(

Fit a 1D Gaussian to data.

Parameters
----------
x : array_like
    The x values.
y : array_like
    The y values.
n_threads : int, optional
    The number of threads to use. Default is 4.
)",
        py::arg("x"), py::arg("y"), py::arg("n_threads") = 4);

    m.def(
        "fit_gaus",
        [](py::array_t<double, py::array::c_style | py::array::forcecast> x,
           py::array_t<double, py::array::c_style | py::array::forcecast> y,
           py::array_t<double, py::array::c_style | py::array::forcecast> y_err,
           int n_threads) {
            if (y.ndim() == 3) {
                // Allocate memory for the output
                // Need to have pointers to allow python to manage
                // the memory
                auto par = new NDArray<double, 3>({y.shape(0), y.shape(1), 3});
                auto par_err =
                    new NDArray<double, 3>({y.shape(0), y.shape(1), 3});
                auto chi2 = new NDArray<double, 2>({y.shape(0), y.shape(1)});

                // Make views of the numpy arrays
                auto y_view = make_view_3d(y);
                auto y_view_err = make_view_3d(y_err);
                auto x_view = make_view_1d(x);

                aare::fit_gaus(x_view, y_view, y_view_err, par->view(),
                               par_err->view(), chi2->view(), n_threads);

                return py::dict("par"_a = return_image_data(par),
                                "par_err"_a = return_image_data(par_err),
                                "chi2"_a = return_image_data(chi2),
                                "Ndf"_a = y.shape(2) - 3);
            } else if (y.ndim() == 1) {
                // Allocate memory for the output
                // Need to have pointers to allow python to manage
                // the memory
                auto par = new NDArray<double, 1>({3});
                auto par_err = new NDArray<double, 1>({3});

                // Decode the numpy arrays
                auto y_view = make_view_1d(y);
                auto y_view_err = make_view_1d(y_err);
                auto x_view = make_view_1d(x);

                double chi2 = 0;
                aare::fit_gaus(x_view, y_view, y_view_err, par->view(),
                               par_err->view(), chi2);

                return py::dict("par"_a = return_image_data(par),
                                "par_err"_a = return_image_data(par_err),
                                "chi2"_a = chi2, "Ndf"_a = y.size() - 3);

            } else {
                throw std::runtime_error("Data must be 1D or 3D");
            }
        },
        R"(

Fit a 1D Gaussian to data with error estimates.

Parameters
----------
x : array_like
    The x values.
y : array_like
    The y values.
y_err : array_like
    The error in the y values.
n_threads : int, optional
    The number of threads to use. Default is 4.
)",
        py::arg("x"), py::arg("y"), py::arg("y_err"), py::arg("n_threads") = 4);

    m.def(
        "fit_pol1",
        [](py::array_t<double, py::array::c_style | py::array::forcecast> x,
           py::array_t<double, py::array::c_style | py::array::forcecast> y,
           int n_threads) {
            if (y.ndim() == 3) {
                auto par = new NDArray<double, 3>{};

                auto x_view = make_view_1d(x);
                auto y_view = make_view_3d(y);
                *par = aare::fit_pol1(x_view, y_view, n_threads);
                return return_image_data(par);
            } else if (y.ndim() == 1) {
                auto par = new NDArray<double, 1>{};
                auto x_view = make_view_1d(x);
                auto y_view = make_view_1d(y);
                *par = aare::fit_pol1(x_view, y_view);
                return return_image_data(par);
            } else {
                throw std::runtime_error("Data must be 1D or 3D");
            }
        },
        py::arg("x"), py::arg("y"), py::arg("n_threads") = 4);

    m.def(
        "fit_pol1",
        [](py::array_t<double, py::array::c_style | py::array::forcecast> x,
           py::array_t<double, py::array::c_style | py::array::forcecast> y,
           py::array_t<double, py::array::c_style | py::array::forcecast> y_err,
           int n_threads) {
            if (y.ndim() == 3) {
                auto par = new NDArray<double, 3>({y.shape(0), y.shape(1), 2});

                auto par_err =
                    new NDArray<double, 3>({y.shape(0), y.shape(1), 2});

                auto y_view = make_view_3d(y);
                auto y_view_err = make_view_3d(y_err);
                auto x_view = make_view_1d(x);

                auto chi2 = new NDArray<double, 2>({y.shape(0), y.shape(1)});

                aare::fit_pol1(x_view, y_view, y_view_err, par->view(),
                               par_err->view(), chi2->view(), n_threads);
                return py::dict("par"_a = return_image_data(par),
                                "par_err"_a = return_image_data(par_err),
                                "chi2"_a = return_image_data(chi2),
                                "Ndf"_a = y.shape(2) - 2);

            } else if (y.ndim() == 1) {
                auto par = new NDArray<double, 1>({2});
                auto par_err = new NDArray<double, 1>({2});

                auto y_view = make_view_1d(y);
                auto y_view_err = make_view_1d(y_err);
                auto x_view = make_view_1d(x);

                double chi2 = 0;

                aare::fit_pol1(x_view, y_view, y_view_err, par->view(),
                               par_err->view(), chi2);
                return py::dict("par"_a = return_image_data(par),
                                "par_err"_a = return_image_data(par_err),
                                "chi2"_a = chi2, "Ndf"_a = y.size() - 2);

            } else {
                throw std::runtime_error("Data must be 1D or 3D");
            }
        },
        R"(
Fit a 1D polynomial to data with error estimates.

Parameters
----------
x : array_like
    The x values.
y : array_like
    The y values.
y_err : array_like
    The error in the y values.
n_threads : int, optional
    The number of threads to use. Default is 4.
)",
        py::arg("x"), py::arg("y"), py::arg("y_err"), py::arg("n_threads") = 4);

    //=========
    m.def(
        "fit_scurve",
        [](py::array_t<double, py::array::c_style | py::array::forcecast> x,
           py::array_t<double, py::array::c_style | py::array::forcecast> y,
           int n_threads) {
            if (y.ndim() == 3) {
                auto par = new NDArray<double, 3>{};

                auto x_view = make_view_1d(x);
                auto y_view = make_view_3d(y);
                *par = aare::fit_scurve(x_view, y_view, n_threads);
                return return_image_data(par);
            } else if (y.ndim() == 1) {
                auto par = new NDArray<double, 1>{};
                auto x_view = make_view_1d(x);
                auto y_view = make_view_1d(y);
                *par = aare::fit_scurve(x_view, y_view);
                return return_image_data(par);
            } else {
                throw std::runtime_error("Data must be 1D or 3D");
            }
        },
        py::arg("x"), py::arg("y"), py::arg("n_threads") = 4);

    m.def(
        "fit_scurve",
        [](py::array_t<double, py::array::c_style | py::array::forcecast> x,
           py::array_t<double, py::array::c_style | py::array::forcecast> y,
           py::array_t<double, py::array::c_style | py::array::forcecast> y_err,
           int n_threads) {
            if (y.ndim() == 3) {
                auto par = new NDArray<double, 3>({y.shape(0), y.shape(1), 6});

                auto par_err =
                    new NDArray<double, 3>({y.shape(0), y.shape(1), 6});

                auto y_view = make_view_3d(y);
                auto y_view_err = make_view_3d(y_err);
                auto x_view = make_view_1d(x);

                auto chi2 = new NDArray<double, 2>({y.shape(0), y.shape(1)});

                aare::fit_scurve(x_view, y_view, y_view_err, par->view(),
                                 par_err->view(), chi2->view(), n_threads);
                return py::dict("par"_a = return_image_data(par),
                                "par_err"_a = return_image_data(par_err),
                                "chi2"_a = return_image_data(chi2),
                                "Ndf"_a = y.shape(2) - 2);

            } else if (y.ndim() == 1) {
                auto par = new NDArray<double, 1>({2});
                auto par_err = new NDArray<double, 1>({2});

                auto y_view = make_view_1d(y);
                auto y_view_err = make_view_1d(y_err);
                auto x_view = make_view_1d(x);

                double chi2 = 0;

                aare::fit_scurve(x_view, y_view, y_view_err, par->view(),
                                 par_err->view(), chi2);
                return py::dict("par"_a = return_image_data(par),
                                "par_err"_a = return_image_data(par_err),
                                "chi2"_a = chi2, "Ndf"_a = y.size() - 2);

            } else {
                throw std::runtime_error("Data must be 1D or 3D");
            }
        },
        R"(
Fit a 1D polynomial to data with error estimates.

Parameters
----------
x : array_like
    The x values.
y : array_like
    The y values.
y_err : array_like
    The error in the y values.
n_threads : int, optional
    The number of threads to use. Default is 4.
)",
        py::arg("x"), py::arg("y"), py::arg("y_err"), py::arg("n_threads") = 4);

    m.def(
        "fit_scurve2",
        [](py::array_t<double, py::array::c_style | py::array::forcecast> x,
           py::array_t<double, py::array::c_style | py::array::forcecast> y,
           int n_threads) {
            if (y.ndim() == 3) {
                auto par = new NDArray<double, 3>{};

                auto x_view = make_view_1d(x);
                auto y_view = make_view_3d(y);
                *par = aare::fit_scurve2(x_view, y_view, n_threads);
                return return_image_data(par);
            } else if (y.ndim() == 1) {
                auto par = new NDArray<double, 1>{};
                auto x_view = make_view_1d(x);
                auto y_view = make_view_1d(y);
                *par = aare::fit_scurve2(x_view, y_view);
                return return_image_data(par);
            } else {
                throw std::runtime_error("Data must be 1D or 3D");
            }
        },
        py::arg("x"), py::arg("y"), py::arg("n_threads") = 4);

    m.def(
        "fit_scurve2",
        [](py::array_t<double, py::array::c_style | py::array::forcecast> x,
           py::array_t<double, py::array::c_style | py::array::forcecast> y,
           py::array_t<double, py::array::c_style | py::array::forcecast> y_err,
           int n_threads) {
            if (y.ndim() == 3) {
                auto par = new NDArray<double, 3>({y.shape(0), y.shape(1), 6});

                auto par_err =
                    new NDArray<double, 3>({y.shape(0), y.shape(1), 6});

                auto y_view = make_view_3d(y);
                auto y_view_err = make_view_3d(y_err);
                auto x_view = make_view_1d(x);

                auto chi2 = new NDArray<double, 2>({y.shape(0), y.shape(1)});

                aare::fit_scurve2(x_view, y_view, y_view_err, par->view(),
                                  par_err->view(), chi2->view(), n_threads);
                return py::dict("par"_a = return_image_data(par),
                                "par_err"_a = return_image_data(par_err),
                                "chi2"_a = return_image_data(chi2),
                                "Ndf"_a = y.shape(2) - 2);

            } else if (y.ndim() == 1) {
                auto par = new NDArray<double, 1>({6});
                auto par_err = new NDArray<double, 1>({6});

                auto y_view = make_view_1d(y);
                auto y_view_err = make_view_1d(y_err);
                auto x_view = make_view_1d(x);

                double chi2 = 0;

                aare::fit_scurve2(x_view, y_view, y_view_err, par->view(),
                                  par_err->view(), chi2);
                return py::dict("par"_a = return_image_data(par),
                                "par_err"_a = return_image_data(par_err),
                                "chi2"_a = chi2, "Ndf"_a = y.size() - 2);

            } else {
                throw std::runtime_error("Data must be 1D or 3D");
            }
        },
        R"(
Fit a 1D polynomial to data with error estimates.

Parameters
----------
x : array_like
    The x values.
y : array_like
    The y values.
y_err : array_like
    The error in the y values.
n_threads : int, optional
    The number of threads to use. Default is 4.
)",
        py::arg("x"), py::arg("y"), py::arg("y_err"), py::arg("n_threads") = 4);
}