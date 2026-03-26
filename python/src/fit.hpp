// SPDX-License-Identifier: MPL-2.0
#include <cstdint>
#include <filesystem>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include <pybind11/stl_bind.h>

#include "aare/Fit.hpp"
#include "aare/FitModel.hpp"
#include "aare/Models.hpp"
#include "aare/Chi2.hpp"

namespace py = pybind11;
using namespace pybind11::literals;

template <typename Model, typename FCN>
py::object fit_dispatch(
    const aare::FitModel<Model>& model,
    py::array_t<double, py::array::c_style | py::array::forcecast> x,
    py::array_t<double, py::array::c_style | py::array::forcecast> y,
    py::object y_err_obj,
    int n_threads);

template <typename Model>
void bind_fit_model(py::module& m, const char* name) {
    using FM = aare::FitModel<Model>;
    using FCN = aare::func::Chi2Model1DGrad<Model>;
    py::class_<FM>(m, name)
        .def(py::init<unsigned int, unsigned int, double, bool>(),
             py::arg("strategy")       = 0,
             py::arg("max_calls")      = 100,
             py::arg("tolerance")      = 0.5,
             py::arg("compute_errors") = false)
        .def("SetParLimits", &FM::SetParLimits, py::arg("idx"), py::arg("lo"), py::arg("hi"))
        .def("FixParameter", &FM::FixParameter, py::arg("idx"), py::arg("val"))
        .def("ReleaseParameter", &FM::ReleaseParameter, py::arg("idx"))
        .def("SetParameter", &FM::SetParameter, py::arg("idx"), py::arg("val"))
        .def_property("max_calls", &FM::max_calls, &FM::SetMaxCalls)
        .def_property("tolerance", &FM::tolerance, &FM::SetTolerance)
        .def_property("compute_errors", &FM::compute_errors, &FM::SetComputeErrors)
        .def("__call__",
            [](const FM& /*self*/,
                py::array_t<double, py::array::c_style | py::array::forcecast> x,
                py::array_t<double, py::array::c_style | py::array::forcecast> par)
            {
                auto x_view = make_view_1d(x);
                auto p_view = make_view_1d(par);

                std::vector<double> pvec(p_view.begin(), p_view.end());

                auto* result = new aare::NDArray<double, 1>({x_view.size()});
                for (ssize_t i = 0; i < x_view.size(); ++i)
                    (*result)(i) = Model::eval(x_view[i], pvec);

                return return_image_data(result);
            },
            py::arg("x"), py::arg("par"))
        .def("fit",
            [](const FM& self,
            py::array_t<double, py::array::c_style | py::array::forcecast> x,
            py::array_t<double, py::array::c_style | py::array::forcecast> y,
            py::object y_err_obj,
            int n_threads) -> py::object
            {
                return fit_dispatch<Model, FCN>(self, x, y, y_err_obj, n_threads);
            },
            R"doc(
            Fit this model to 1D or 3D data using Minuit2.

            Parameters
            ----------
            x : array_like, shape (n_scan,)
                Scan points.
            y : array_like, shape (n_scan,) or (rows, cols, n_scan)
                Measured data.
            y_err : array_like or None
                Per-point uncertainties. None for unweighted fit.
            n_threads : int
                Number of threads for 3D parallel loop.
            )doc",
            py::arg("x"),
            py::arg("y"),
            py::arg("y_err")    = py::none(),
            py::arg("n_threads") = 4);
}

template <typename Model>
py::dict pack_1d_result_dict(const aare::NDArray<double, 1>& result,
                             bool compute_errors)
{
    constexpr std::size_t npar = Model::npar;

    auto res = result.view();

    auto par_out  = new NDArray<double, 1>({npar}, 0.0);
    auto chi2_out = new NDArray<double, 1>({1},    0.0);

    auto par_view  = par_out->view();
    auto chi2_view = chi2_out->view();

    for (std::size_t i = 0; i < npar; ++i) {
        par_view(i) = res(i);
    }

    if (compute_errors) {
        auto err_out  = new NDArray<double, 1>({npar}, 0.0);
        auto err_view = err_out->view();

        for (std::size_t i = 0; i < npar; ++i) {
            err_view(i) = res(npar + i);
        }

        chi2_view(0) = res(2 * npar);

        return py::dict(
            "par"_a     = return_image_data(par_out),
            "par_err"_a = return_image_data(err_out),
            "chi2"_a    = return_image_data(chi2_out));
    } else {
        chi2_view(0) = res(npar);

        return py::dict(
            "par"_a  = return_image_data(par_out),
            "chi2"_a = return_image_data(chi2_out));
    }
}

// Helper: typed dispatch for one Model, handles 1D/3D + y_err logic
template <typename Model, typename FCN>
py::object fit_dispatch(
    const aare::FitModel<Model>& model,
    py::array_t<double, py::array::c_style | py::array::forcecast> x,
    py::array_t<double, py::array::c_style | py::array::forcecast> y,
    py::object y_err_obj,
    int n_threads)
{
    constexpr std::size_t npar = Model::npar;

    if (y.ndim() == 3) {
        auto par_out = new NDArray<double, 3>({y.shape(0), y.shape(1), npar}, 0.0);
        auto chi2_out= new NDArray<double, 2>({y.shape(0), y.shape(1)}, 0.0);

        auto x_view = make_view_1d(x);
        auto y_view = make_view_3d(y);

        if (!y_err_obj.is_none()) {
            auto y_err = py::cast<py::array_t<double,
                py::array::c_style | py::array::forcecast>>(y_err_obj);

            if (y_err.ndim() != 3) {
                throw std::runtime_error("For 3D input y, y_err must also be 3D.");
            }
            
            auto err_out = new NDArray<double, 3>({y.shape(0), y.shape(1), npar}, 0.0);
            auto y_view_err = make_view_3d(y_err);

            aare::fit_3d<Model, FCN>(model, x_view, y_view, y_view_err, 
                            par_out->view(), err_out->view(), chi2_out->view(), n_threads);
            
            if (model.compute_errors()) {
                return py::dict("par"_a     = return_image_data(par_out),
                                "par_err"_a = return_image_data(err_out),
                                "chi2"_a    = return_image_data(chi2_out));
            } else {
                delete err_out;
                return py::dict("par"_a  = return_image_data(par_out),
                                "chi2"_a = return_image_data(chi2_out));
            }
        } else {

            NDView<double, 3> dummy_err{};
            NDView<double, 3> dummy_err_out{};

            aare::fit_3d<Model, FCN>(model, x_view, y_view, dummy_err, 
                            par_out->view(), dummy_err_out, chi2_out->view(), n_threads);
                
            return py::dict("par"_a = return_image_data(par_out),
                            "chi2"_a = return_image_data(chi2_out));
        }
    } else if (y.ndim() == 1) {
        NDArray<double, 1> result{};

        auto x_view = make_view_1d(x);
        auto y_view = make_view_1d(y);

        if (!y_err_obj.is_none()) {
            auto y_err = py::cast<py::array_t<double,
                py::array::c_style | py::array::forcecast>>(y_err_obj);

            if (y_err.ndim() != 1) {
                throw std::runtime_error("For 1D input y, y_err must also be 1D.");
            }

            auto y_view_err = make_view_1d(y_err);
            result = aare::fit_pixel<Model, FCN>(model, x_view, y_view, y_view_err);
        } else {
            result = aare::fit_pixel<Model, FCN>(model, x_view, y_view);
        }

        return pack_1d_result_dict<Model>(result, model.compute_errors());

    } else {
        throw std::runtime_error("Data must be 1D or 3D.");
    }
}

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


    // ── Bind model classes ──────────────────────────────────────────
    bind_fit_model<aare::model::Gaussian>(m, "Gaussian");
    bind_fit_model<aare::model::RisingScurve>(m, "RisingScurve");
    bind_fit_model<aare::model::FallingScurve>(m, "FallingScurve");
    bind_fit_model<aare::model::Pol1>(m, "Pol1");
    bind_fit_model<aare::model::Pol2>(m, "Pol2");

    m.def("fit",
        [](py::object model_obj,
           py::array_t<double, py::array::c_style | py::array::forcecast> x,
           py::array_t<double, py::array::c_style | py::array::forcecast> y,
           py::object y_err_obj,
           int n_threads) -> py::object
        {
            using namespace aare::model;
            using namespace aare::func;
            
            // ── Polynomial of degree 1 ───────
            if(py::isinstance< aare::FitModel<Pol1> >(model_obj)) {
                const auto& mdl = model_obj.cast< const aare::FitModel<Pol1>& >();
                return fit_dispatch<Pol1, Chi2Pol1>(mdl, x, y, y_err_obj, n_threads); 
            }

            // ── Polynomial of degree 2 ───────
            if(py::isinstance< aare::FitModel<Pol2> >(model_obj)) {
                const auto& mdl = model_obj.cast< const aare::FitModel<Pol2>& >();
                return fit_dispatch<Pol2, Chi2Pol2>(mdl, x, y, y_err_obj, n_threads); 
            }
            // ── Gaussian ───────
            if(py::isinstance< aare::FitModel<Gaussian> >(model_obj)) {
                const auto& mdl = model_obj.cast< const aare::FitModel<Gaussian>& >();
                return fit_dispatch<Gaussian, Chi2Gaussian>(mdl, x, y, y_err_obj, n_threads); 
            }

            // ── Rising Scurve ───────
            if(py::isinstance< aare::FitModel<RisingScurve> >(model_obj)) {
                const auto& mdl = model_obj.cast< const aare::FitModel<RisingScurve>& >();
                return fit_dispatch<RisingScurve, Chi2RisingScurve>(mdl, x, y, y_err_obj, n_threads); 
            }

            // ── Falling Scurve ───────
            if(py::isinstance< aare::FitModel<FallingScurve> >(model_obj)) {
                const auto& mdl = model_obj.cast< const aare::FitModel<FallingScurve>& >();
                return fit_dispatch<FallingScurve, Chi2FallingScurve>(mdl, x, y, y_err_obj, n_threads); 
            }

            throw std::runtime_error(
                "Unknown model type. Expected Pol1, Pol2, Gaussian, RisingScurve or FallingScurve."
            );
        },
        R"(
        Fit a model to 1D or 3D data using Minuit2.
 
        Parameters
        ----------
        model : Pol1, Pol2, Gaussian, RisingScurve, or FallingScurve
            Configured model object.  User-set limits, fixed parameters,
            and start values take precedence over automatic estimates.
        x : array_like, shape (n_scan,)
            Scan points (e.g. energy or threshold values).
        y : array_like, shape (n_scan,) or (rows, cols, n_scan)
            Measured data.  1D for a single pixel, 3D for a detector image.
        y_err : array_like or None
            Per-point uncertainties.  Same shape as y.  None → unweighted fit.
        n_threads : int
            Number of threads for the 3D parallel loop.
 
        Returns
        -------
        For 1D input:
            numpy array of shape (2*npar+1,) if compute_errors else (npar+1,).
            Layout: [params..., (errors...,) chi2].
 
        For 3D input:
            dict with keys:
              "par"     : (rows, cols, npar) fitted parameters.
              "par_err" : (rows, cols, npar) parameter errors (if compute_errors).
              "chi2"    : (rows, cols)       chi-squared per pixel.
        )",
        py::arg("model"),
        py::arg("x"),
        py::arg("y"), 
        py::arg("y_err") = py::none(), 
        py::arg("n_threads") = 4
    );
}