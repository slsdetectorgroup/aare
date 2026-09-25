// SPDX-License-Identifier: MPL-2.0
#include <cstdint>
#include <filesystem>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include <pybind11/stl_bind.h>

#include "aare/Fit.hpp"
#include "aare/FitModel.hpp"
#include "aare/Models.hpp"

namespace py = pybind11;
using namespace pybind11::literals;

template <typename Model>
py::object
fit_dispatch(const aare::FitModel<Model> &model,
             py::array_t<double, py::array::c_style | py::array::forcecast> x,
             py::array_t<double, py::array::c_style | py::array::forcecast> y,
             py::object y_err_obj, int n_threads);

template <typename Model> void bind_fit_model(py::module &m, const char *name) {
    using FM = aare::FitModel<Model>;
    py::class_<FM>(m, name)
        .def(py::init<unsigned int, unsigned int, double, bool,
                      aare::Minimizer>(),
             py::arg("strategy") = 0, py::arg("max_calls") = 100,
             py::arg("tolerance") = 0.5, py::arg("compute_errors") = false,
             py::arg("minimizer") = aare::Minimizer::Migrad,
             R"doc(
            Fit configuration for this model.

            Parameters
            ----------
            strategy : int
                Minuit2 strategy (0 = fast, 1 = Minuit2's default). Ignored
                by ``Minimizer.LevenbergMarquardt``.
            max_calls : int
                Work budget per pixel: Minuit2 function calls, or model
                evaluations for ``Minimizer.LevenbergMarquardt``. 0 selects
                Minuit's default budget.
            tolerance : float
                EDM tolerance in Minuit's convention. Migrad and
                LevenbergMarquardt stop below ``0.002 * tolerance``, Fumili
                below ``1e-4 * tolerance``.
            compute_errors : bool
                Also return parameter errors: from Hesse with Migrad, from
                the linearised covariance with Fumili and LevenbergMarquardt.
                Fixed parameters and parameters ending on a limit report 0.
            minimizer : Minimizer
                ``Minimizer.Migrad`` (default), ``Minimizer.Fumili`` or
                ``Minimizer.LevenbergMarquardt``.
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
        .def_property("minimizer", &FM::minimizer, &FM::SetMinimizer)
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
                return fit_dispatch<Model>(self, x, y, y_err_obj, n_threads);
            },
            R"doc(
            Fit this model to 1D or 3D data using Minuit2.

            The minimizer is selected by the ``minimizer`` property
            (``Minimizer.Migrad`` by default, ``Minimizer.Fumili`` or
            ``Minimizer.LevenbergMarquardt``).

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
template <typename Model>
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

            aare::fit_3d<Model>(model, x_view, y_view, y_view_err,
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

            aare::fit_3d<Model>(model, x_view, y_view, dummy_err,
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
            result = aare::fit_pixel<Model>(model, x_view, y_view, y_view_err);
        } else {
            result = aare::fit_pixel<Model>(model, x_view, y_view);
        }

        return pack_1d_result_dict<Model>(result, model.compute_errors());

    } else {
        throw std::runtime_error("Data must be 1D or 3D.");
    }
}

// Resolve the model type behind a Python object and run the fit.
py::object dispatch_any_model(
    py::object model_obj,
    py::array_t<double, py::array::c_style | py::array::forcecast> x,
    py::array_t<double, py::array::c_style | py::array::forcecast> y,
    py::object y_err_obj, int n_threads) {
    using namespace aare::model;

#define AARE_DISPATCH_MODEL(Model)                                             \
    if (py::isinstance<aare::FitModel<Model>>(model_obj)) {                    \
        const auto &mdl = model_obj.cast<const aare::FitModel<Model> &>();     \
        return fit_dispatch<Model>(mdl, x, y, y_err_obj, n_threads);           \
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

void define_fit_bindings(py::module &m) {
    // The enum must exist before it is used as a constructor default below.
    py::enum_<aare::Minimizer>(m, "Minimizer", R"doc(
        Minimizer used by the fit models.

        ``Migrad`` (default) is Minuit2's variable-metric minimizer. With
        ``compute_errors`` its parameter errors come from Hesse.

        ``Fumili`` is Minuit2's Gauss-Newton minimizer with a trust region.
        It takes the gradient and a linearised Hessian from the analytic
        derivatives of the model and usually needs far fewer function
        evaluations than Migrad. Its parameter errors come from the
        covariance of the linearised Hessian, without an extra Hesse step.
        Minuit2's Fumili cannot converge once a two-sided limit becomes
        active; a pixel without a valid Fumili minimum is refitted with
        Migrad.

        ``LevenbergMarquardt`` is the built-in damped Gauss-Newton solver. It
        uses the analytic Jacobian, reflects steps at parameter limits and
        reuses its buffers between pixels, so data cubes are fitted without
        per-pixel allocations. Its parameter errors are
        ``sqrt(diag((J^T J)^-1))``; ``max_calls`` counts model evaluations
        and ``strategy`` is ignored.

        With every minimizer, fixed parameters and parameters that end on a
        limit report an error of 0 and a failed fit returns zeros.
        )doc")
        .value("Migrad", aare::Minimizer::Migrad)
        .value("Fumili", aare::Minimizer::Fumili)
        .value("LevenbergMarquardt", aare::Minimizer::LevenbergMarquardt);

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
            return dispatch_any_model(model_obj, x, y, y_err_obj, n_threads);
        },
        R"(
        Fit a model to 1D or 3D data with the minimizer selected by the
        model's ``minimizer`` property.
 
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
            shape (1,) or (rows, cols).  ``par_err`` holds the parameter
            errors when ``compute_errors`` is enabled; fixed parameters and
            parameters on a limit report 0.  A failed fit returns zeros.
        )",
        py::arg("model"), py::arg("x"), py::arg("y"),
        py::arg("y_err") = py::none(), py::arg("n_threads") = 4);
}
