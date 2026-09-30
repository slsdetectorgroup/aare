// SPDX-License-Identifier: MPL-2.0
#include <array>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include <pybind11/stl_bind.h>

#include "aare/Fit.hpp"
#include "aare/FitModel.hpp"
#include "aare/Models.hpp"

namespace py = pybind11;
using namespace pybind11::literals;

// Input arrays are converted, so that the fit can be called with any dtype.
using DoubleArray =
    py::array_t<double, py::array::c_style | py::array::forcecast>;

template <typename Model>
py::dict fit_dispatch(const aare::FitModel<Model> &model, DoubleArray x,
                      DoubleArray y, py::object y_err_obj, int n_threads);

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
                by ``Minimizer.LevenbergMarquardt`` and
                ``Minimizer.VarPro``.
            max_calls : int
                Work budget per pixel: Minuit2 function calls, or model
                evaluations for ``Minimizer.LevenbergMarquardt`` and
                ``Minimizer.VarPro`` (one per iteration, two
                when a step crosses a limit). 0 selects Minuit's default
                budget.
            tolerance : float
                EDM tolerance in Minuit's convention. Migrad,
                LevenbergMarquardt and VarPro stop below
                ``0.002 * tolerance``, Fumili below ``1e-4 * tolerance``.
            compute_errors : bool
                Also return parameter errors: from Hesse with Migrad, from
                the linearised covariance with Fumili, LevenbergMarquardt
                and VarPro. Fixed parameters and parameters
                ending on a limit report 0.
            minimizer : Minimizer
                ``Minimizer.Migrad`` (default), ``Minimizer.Fumili``,
                ``Minimizer.LevenbergMarquardt`` or
                ``Minimizer.VarPro``.
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
            [](const FM & /*self*/, DoubleArray x, DoubleArray par) {
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
            [](const FM &self, DoubleArray x, DoubleArray y,
               py::object y_err_obj, int n_threads) {
                return fit_dispatch<Model>(self, x, y, y_err_obj, n_threads);
            },
            R"doc(
            Fit this model to 1D or 3D data.

            The minimizer is selected by the ``minimizer`` property
            (``Minimizer.Migrad`` by default, ``Minimizer.Fumili``,
            ``Minimizer.LevenbergMarquardt`` or
            ``Minimizer.VarPro``).

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

// Fit one model to 1D or 3D data and pack the result: "par", "par_err" when
// the model computes errors, and "chi2".
template <typename Model>
py::dict fit_dispatch(const aare::FitModel<Model> &model, DoubleArray x,
                      DoubleArray y, py::object y_err_obj, int n_threads) {
    constexpr auto npar = static_cast<ssize_t>(Model::npar);
    const bool want_errors = model.compute_errors();
    const bool weighted = !y_err_obj.is_none();
    DoubleArray y_err;
    if (weighted)
        y_err = py::cast<DoubleArray>(y_err_obj);
    auto x_view = make_view_1d(x);
    py::dict result;

    if (y.ndim() == 3) {
        const ssize_t rows = y.shape(0);
        const ssize_t cols = y.shape(1);
        using Cube = NDArray<double, 3>;
        using Image = NDArray<double, 2>;
        const std::array<ssize_t, 3> par_shape{rows, cols, npar};
        auto par = std::make_unique<Cube>(par_shape, 0.0);
        auto par_err =
            want_errors ? std::make_unique<Cube>(par_shape, 0.0) : nullptr;
        auto chi2 =
            std::make_unique<Image>(std::array<ssize_t, 2>{rows, cols}, 0.0);

        aare::fit_3d<Model>(
            model, x_view, make_view_3d(y),
            weighted ? make_view_3d(y_err) : NDView<double, 3>{}, par->view(),
            par_err ? par_err->view() : NDView<double, 3>{}, chi2->view(),
            n_threads);

        result["par"] = return_image_data(par.release());
        if (par_err)
            result["par_err"] = return_image_data(par_err.release());
        result["chi2"] = return_image_data(chi2.release());
    } else if (y.ndim() == 1) {
        // [par..., (err...,) chi2], see fit_pixel
        const auto flat = aare::fit_pixel<Model>(
            model, x_view, make_view_1d(y),
            weighted ? make_view_1d(y_err) : NDView<double, 1>{});

        result["par"] = py::array_t<double>(npar, flat.data());
        if (want_errors)
            result["par_err"] = py::array_t<double>(npar, flat.data() + npar);
        result["chi2"] = py::array_t<double>(1, flat.data() + flat.size() - 1);
    } else {
        throw std::runtime_error("Data must be 1D or 3D.");
    }
    return result;
}

// Resolve the model type behind a Python object and run the fit.
py::dict dispatch_any_model(py::object model_obj, DoubleArray x, DoubleArray y,
                            py::object y_err_obj, int n_threads) {
#define AARE_DISPATCH_MODEL(Model)                                             \
    if (py::isinstance<aare::FitModel<aare::model::Model>>(model_obj)) {       \
        const auto &mdl =                                                      \
            model_obj.cast<const aare::FitModel<aare::model::Model> &>();      \
        return fit_dispatch(mdl, x, y, y_err_obj, n_threads);                  \
    }
    AARE_FOR_EACH_FIT_MODEL(AARE_DISPATCH_MODEL)
#undef AARE_DISPATCH_MODEL

#define AARE_MODEL_NAME(Model) " " #Model
    throw std::runtime_error(
        "Unknown model type. Expected one of:" AARE_FOR_EACH_FIT_MODEL(
            AARE_MODEL_NAME));
#undef AARE_MODEL_NAME
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

        ``VarPro`` is the built-in variable projection solver.
        Every bundled model is linear in some of its parameters (the
        amplitude of a Gaussian, the baseline and amplitude of an S-curve,
        every parameter of a polynomial); each trial point solves those
        exactly and the Levenberg-Marquardt iteration runs over the
        remaining nonlinear parameters only. It needs fewer evaluations than
        ``LevenbergMarquardt`` and cannot be misled by poor start values of
        the linear parameters, which it ignores. A pixel whose linear
        solution violates a limit on a linear parameter, or that does not
        converge, is refitted with ``LevenbergMarquardt``. Its parameter
        errors are the same Gauss-Newton estimates; ``max_calls`` counts
        evaluations and ``strategy`` is ignored.

        With every minimizer, fixed parameters and parameters that end on a
        limit report an error of 0 and a failed fit returns zeros.
        )doc")
        .value("Migrad", aare::Minimizer::Migrad)
        .value("Fumili", aare::Minimizer::Fumili)
        .value("LevenbergMarquardt", aare::Minimizer::LevenbergMarquardt)
        .value("VarPro", aare::Minimizer::VarPro);

    // ── Bind model classes ──────────────────────────────────────────
#define AARE_BIND_FIT_MODEL(Model)                                             \
    bind_fit_model<aare::model::Model>(m, #Model);
    AARE_FOR_EACH_FIT_MODEL(AARE_BIND_FIT_MODEL)
#undef AARE_BIND_FIT_MODEL

    m.def("fit", &dispatch_any_model,
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
