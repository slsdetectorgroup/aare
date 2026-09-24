// SPDX-License-Identifier: MPL-2.0
#include <cstdint>
#include <filesystem>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include <pybind11/stl_bind.h>

#include "aare/Fit.hpp"
#include "aare/FitModel.hpp"
#include "aare/Models.hpp"
#ifdef AARE_MINUIT2
#include "aare/FitMinuit2.hpp"
#endif

namespace py = pybind11;
using namespace pybind11::literals;

// Backend tags select which fit functions serve a Python call. The built-in
// Levenberg-Marquardt solver is always available; the Minuit2 backend only
// when aare was configured with AARE_MINUIT2=ON.
struct LmBackend {
    template <typename Model, typename... Args>
    static aare::NDArray<double, 1> fit_pixel(Args &&...args) {
        return aare::fit_pixel<Model>(std::forward<Args>(args)...);
    }
    template <typename Model, typename... Args>
    static void fit_3d(Args &&...args) {
        aare::fit_3d<Model>(std::forward<Args>(args)...);
    }
};

#ifdef AARE_MINUIT2
struct Minuit2Backend {
    template <typename Model, typename... Args>
    static aare::NDArray<double, 1> fit_pixel(Args &&...args) {
        return aare::minuit2::fit_pixel<Model>(std::forward<Args>(args)...);
    }
    template <typename Model, typename... Args>
    static void fit_3d(Args &&...args) {
        aare::minuit2::fit_3d<Model>(std::forward<Args>(args)...);
    }
};
#endif

template <typename Model, typename Backend>
py::object
fit_dispatch(const aare::FitModel<Model> &model,
             py::array_t<double, py::array::c_style | py::array::forcecast> x,
             py::array_t<double, py::array::c_style | py::array::forcecast> y,
             py::object y_err_obj, int n_threads);

template <typename Model> void bind_fit_model(py::module &m, const char *name) {
    using FM = aare::FitModel<Model>;
    py::class_<FM>(m, name)
        .def(py::init<unsigned int, unsigned int, double, bool>(),
             py::arg("strategy") = 0, py::arg("max_calls") = 100,
             py::arg("tolerance") = 0.5, py::arg("compute_errors") = false,
             R"doc(
            Fit configuration for this model.

            Parameters
            ----------
            strategy : int
                Accepted for compatibility and ignored by the built-in solver.
            max_calls : int
                Maximum number of model evaluations per pixel.
            tolerance : float
                EDM tolerance (Minuit convention): the fit stops when the
                estimated distance to the minimum is below 0.002 * tolerance.
            compute_errors : bool
                Also return Gauss-Newton parameter errors. Fixed parameters
                and parameters ending on a limit report an error of 0.
            )doc")
        .def("SetParLimits",
             py::overload_cast<unsigned int, double, double>(&FM::SetParLimits),
             py::arg("idx"), py::arg("lo"), py::arg("hi"))
        .def("SetParLimits",
             py::overload_cast<const std::string &, double, double>(
                 &FM::SetParLimits),
             py::arg("idx"), py::arg("lo"), py::arg("hi"))
        .def("FixParameter",
             py::overload_cast<unsigned int, double>(&FM::FixParameter),
             py::arg("idx"), py::arg("val"))
        .def("FixParameter",
             py::overload_cast<const std::string &, double>(&FM::FixParameter),
             py::arg("idx"), py::arg("val"))
        .def("ReleaseParameter",
             py::overload_cast<unsigned int>(&FM::ReleaseParameter),
             py::arg("idx"))
        .def("ReleaseParameter",
             py::overload_cast<const std::string &>(&FM::ReleaseParameter),
             py::arg("idx"))
        .def("SetParameter",
             py::overload_cast<unsigned int, double>(&FM::SetParameter),
             py::arg("idx"), py::arg("val"))
        .def("SetParameter",
             py::overload_cast<const std::string &, double>(&FM::SetParameter),
             py::arg("idx"), py::arg("val"))
        .def("GetParName", &FM::GetParName, py::arg("idx"))
        .def("GetParNames", &FM::GetParNames)
        .def_property_readonly("par_names", &FM::GetParNames)
        .def_property_readonly("n_par",
                               [](py::object /*cls*/) { return Model::npar; })
        .def_property("max_calls", &FM::max_calls, &FM::SetMaxCalls)
        .def_property("tolerance", &FM::tolerance, &FM::SetTolerance)
        .def_property("compute_errors", &FM::compute_errors,
                      &FM::SetComputeErrors)
        .def(
            "__call__",
            [](const FM & /*self*/,
               py::array_t<double, py::array::c_style | py::array::forcecast> x,
               py::array_t<double, py::array::c_style | py::array::forcecast>
                   par) {
                auto x_view = make_view_1d(x);
                auto p_view = make_view_1d(par);

                std::vector<double> pvec(p_view.begin(), p_view.end());

                auto *result = new aare::NDArray<double, 1>({x_view.size()});
                for (ssize_t i = 0; i < x_view.size(); ++i)
                    (*result)(i) = Model::eval(x_view[i], pvec);

                return return_image_data(result);
            },
            py::arg("x"), py::arg("par"))
        .def(
            "fit",
            [](const FM &self,
               py::array_t<double, py::array::c_style | py::array::forcecast> x,
               py::array_t<double, py::array::c_style | py::array::forcecast> y,
               py::object y_err_obj, int n_threads) -> py::object {
                return fit_dispatch<Model, LmBackend>(self, x, y, y_err_obj,
                                                      n_threads);
            },
            R"doc(
            Fit this model to 1D or 3D data with the built-in
            Levenberg-Marquardt solver.

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
            py::arg("x"), py::arg("y"), py::arg("y_err") = py::none(),
            py::arg("n_threads") = 4);
}

template <typename Model>
py::dict pack_1d_result_dict(const aare::NDArray<double, 1> &result,
                             bool compute_errors) {
    constexpr std::size_t npar = Model::npar;

    auto res = result.view();

    auto par_out = new NDArray<double, 1>({npar}, 0.0);
    auto chi2_out = new NDArray<double, 1>({1}, 0.0);

    auto par_view = par_out->view();
    auto chi2_view = chi2_out->view();

    for (std::size_t i = 0; i < npar; ++i) {
        par_view(i) = res(i);
    }

    if (compute_errors) {
        auto err_out = new NDArray<double, 1>({npar}, 0.0);
        auto err_view = err_out->view();

        for (std::size_t i = 0; i < npar; ++i) {
            err_view(i) = res(npar + i);
        }

        chi2_view(0) = res(2 * npar);

        return py::dict("par"_a = return_image_data(par_out),
                        "par_err"_a = return_image_data(err_out),
                        "chi2"_a = return_image_data(chi2_out));
    } else {
        chi2_view(0) = res(npar);

        return py::dict("par"_a = return_image_data(par_out),
                        "chi2"_a = return_image_data(chi2_out));
    }
}

// Helper: typed dispatch for one Model, handles 1D/3D + y_err logic
template <typename Model, typename Backend>
py::object
fit_dispatch(const aare::FitModel<Model> &model,
             py::array_t<double, py::array::c_style | py::array::forcecast> x,
             py::array_t<double, py::array::c_style | py::array::forcecast> y,
             py::object y_err_obj, int n_threads) {
    constexpr std::size_t npar = Model::npar;

    if (y.ndim() == 3) {
        auto par_out =
            new NDArray<double, 3>({y.shape(0), y.shape(1), npar}, 0.0);
        auto chi2_out = new NDArray<double, 2>({y.shape(0), y.shape(1)}, 0.0);

        auto x_view = make_view_1d(x);
        auto y_view = make_view_3d(y);

        if (!y_err_obj.is_none()) {
            auto y_err = py::cast<
                py::array_t<double, py::array::c_style | py::array::forcecast>>(
                y_err_obj);

            if (y_err.ndim() != 3) {
                throw std::runtime_error(
                    "For 3D input y, y_err must also be 3D.");
            }

            auto err_out =
                new NDArray<double, 3>({y.shape(0), y.shape(1), npar}, 0.0);
            auto y_view_err = make_view_3d(y_err);

            Backend::template fit_3d<Model>(model, x_view, y_view, y_view_err,
                                            par_out->view(), err_out->view(),
                                            chi2_out->view(), n_threads);

            if (model.compute_errors()) {
                return py::dict("par"_a = return_image_data(par_out),
                                "par_err"_a = return_image_data(err_out),
                                "chi2"_a = return_image_data(chi2_out));
            } else {
                delete err_out;
                return py::dict("par"_a = return_image_data(par_out),
                                "chi2"_a = return_image_data(chi2_out));
            }
        } else {

            NDView<double, 3> dummy_err{};
            NDView<double, 3> dummy_err_out{};

            Backend::template fit_3d<Model>(model, x_view, y_view, dummy_err,
                                            par_out->view(), dummy_err_out,
                                            chi2_out->view(), n_threads);

            return py::dict("par"_a = return_image_data(par_out),
                            "chi2"_a = return_image_data(chi2_out));
        }
    } else if (y.ndim() == 1) {
        NDArray<double, 1> result{};

        auto x_view = make_view_1d(x);
        auto y_view = make_view_1d(y);

        if (!y_err_obj.is_none()) {
            auto y_err = py::cast<
                py::array_t<double, py::array::c_style | py::array::forcecast>>(
                y_err_obj);

            if (y_err.ndim() != 1) {
                throw std::runtime_error(
                    "For 1D input y, y_err must also be 1D.");
            }

            auto y_view_err = make_view_1d(y_err);
            result = Backend::template fit_pixel<Model>(model, x_view, y_view,
                                                        y_view_err);
        } else {
            result = Backend::template fit_pixel<Model>(model, x_view, y_view);
        }

        return pack_1d_result_dict<Model>(result, model.compute_errors());

    } else {
        throw std::runtime_error("Data must be 1D or 3D.");
    }
}

// Resolve the model type behind a Python object and dispatch to Backend.
template <typename Backend>
py::object dispatch_any_model(
    py::object model_obj,
    py::array_t<double, py::array::c_style | py::array::forcecast> x,
    py::array_t<double, py::array::c_style | py::array::forcecast> y,
    py::object y_err_obj, int n_threads) {
    using namespace aare::model;

#define AARE_DISPATCH_MODEL(Model)                                             \
    if (py::isinstance<aare::FitModel<Model>>(model_obj)) {                    \
        const auto &mdl = model_obj.cast<const aare::FitModel<Model> &>();     \
        return fit_dispatch<Model, Backend>(mdl, x, y, y_err_obj, n_threads);  \
    }

    AARE_DISPATCH_MODEL(Pol1)
    AARE_DISPATCH_MODEL(Pol2)
    AARE_DISPATCH_MODEL(Gaussian)
    AARE_DISPATCH_MODEL(GaussianErfcPlateau)
    AARE_DISPATCH_MODEL(GaussianChargeSharing)
    AARE_DISPATCH_MODEL(GaussianChargeSharingKb)
    AARE_DISPATCH_MODEL(RisingScurve)
    AARE_DISPATCH_MODEL(FallingScurve)

#undef AARE_DISPATCH_MODEL

    throw std::runtime_error(
        "Unknown model type. Expected Pol1, Pol2, Gaussian, "
        "GaussianErfcPlateau, GaussianChargeSharing, GaussianChargeSharingKb, "
        "RisingScurve or FallingScurve.");
}

void define_fit_bindings(py::module &m, py::module &experimental) {
    // ── Bind model classes ──────────────────────────────────────────
    bind_fit_model<aare::model::Gaussian>(m, "Gaussian");
    bind_fit_model<aare::model::GaussianErfcPlateau>(m, "GaussianErfcPlateau");
    bind_fit_model<aare::model::GaussianChargeSharing>(m,
                                                       "GaussianChargeSharing");
    bind_fit_model<aare::model::GaussianChargeSharingKb>(
        m, "GaussianChargeSharingKb");
    bind_fit_model<aare::model::RisingScurve>(m, "RisingScurve");
    bind_fit_model<aare::model::FallingScurve>(m, "FallingScurve");
    bind_fit_model<aare::model::Pol1>(m, "Pol1");
    bind_fit_model<aare::model::Pol2>(m, "Pol2");

    m.def(
        "fit",
        [](py::object model_obj,
           py::array_t<double, py::array::c_style | py::array::forcecast> x,
           py::array_t<double, py::array::c_style | py::array::forcecast> y,
           py::object y_err_obj, int n_threads) -> py::object {
            return dispatch_any_model<LmBackend>(model_obj, x, y, y_err_obj,
                                                 n_threads);
        },
        R"(
        Fit a model to 1D or 3D data with the built-in Levenberg-Marquardt
        solver.
 
        Parameters
        ----------
        model : object
            Configured model object: Pol1, Pol2, Gaussian, GaussianErfcPlateau,
            GaussianChargeSharing, GaussianChargeSharingKb, RisingScurve or
            FallingScurve.  User-set limits, fixed parameters, and start values
            take precedence over automatic estimates.
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
        dict
            ``par`` holds the fitted parameters, shape (npar,) for 1D input or
            (rows, cols, npar) for 3D input.  ``chi2`` holds the chi-squared,
            shape (1,) or (rows, cols).  ``par_err`` holds the Gauss-Newton
            parameter errors when ``compute_errors`` is enabled; fixed
            parameters and parameters on a limit report 0.  A failed fit
            returns zeros.
        )",
        py::arg("model"), py::arg("x"), py::arg("y"),
        py::arg("y_err") = py::none(), py::arg("n_threads") = 4);

#ifdef AARE_MINUIT2
    experimental.def(
        "fit_minuit2",
        [](py::object model_obj,
           py::array_t<double, py::array::c_style | py::array::forcecast> x,
           py::array_t<double, py::array::c_style | py::array::forcecast> y,
           py::object y_err_obj, int n_threads) -> py::object {
            return dispatch_any_model<Minuit2Backend>(model_obj, x, y,
                                                      y_err_obj, n_threads);
        },
        R"(
        Validation only: fit with the Minuit2 (Migrad + Hesse) backend.

        Available when aare was built with -DAARE_MINUIT2=ON. Takes the same
        arguments and returns the same dictionary as fit().
        )",
        py::arg("model"), py::arg("x"), py::arg("y"),
        py::arg("y_err") = py::none(), py::arg("n_threads") = 4);
#else
    (void)experimental;
#endif
}
