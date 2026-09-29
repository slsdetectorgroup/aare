// SPDX-License-Identifier: MPL-2.0
// Minuit2 backends of the pixel fit: Migrad (MnHesse for the errors) and
// Fumili. This is the only translation unit that includes Minuit2 headers.
#include "FitMinuit2.hpp"
#include "Chi2.hpp"
#include "Minuit2/FunctionMinimum.h"
#include "Minuit2/MnFumiliMinimize.h"
#include "Minuit2/MnHesse.h"
#include "Minuit2/MnMigrad.h"
#include "Minuit2/MnStrategy.h"
#include "Minuit2/MnUserParameterState.h"
#include "Minuit2/MnUserParameters.h"
#include "MinuitSteps.hpp"
#include "aare/Models.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <vector>

namespace aare::detail {

namespace {

// Translate the FitModel state into Minuit2 user parameters for one pixel.
template <typename Model>
ROOT::Minuit2::MnUserParameters
user_parameters(const FitModel<Model> &model, NDView<double, 1> x,
                NDView<double, 1> y,
                const std::array<double, Model::npar> &start) {
    constexpr std::size_t npar = Model::npar;

    double x_range = 0.0;
    double y_range = 0.0;
    double slope_scale = 0.0;
    minuit2::compute_ranges(x, y, x_range, y_range, slope_scale);
    std::array<double, npar> steps{};
    minuit2::Steps<Model>::compute(start, x_range, y_range, slope_scale, steps);

    ROOT::Minuit2::MnUserParameters upar;
    for (std::size_t i = 0; i < npar; ++i) {
        const auto idx = static_cast<unsigned int>(i);
        upar.Add(Model::param_info[i].name, start[i], steps[i]);
        const double lo = model.lower_limit(idx);
        const double hi = model.upper_limit(idx);
        if (std::isfinite(lo) && std::isfinite(hi)) {
            upar.SetLimits(idx, lo, hi);
        } else if (std::isfinite(lo) || std::isfinite(hi)) {
            // Minuit2's transformation for one-sided limits converges markedly
            // slower than the two-sided one, so an open side gets a wide bound.
            const double span = std::max(1e6, 1e3 * std::abs(start[i]));
            if (std::isfinite(lo))
                upar.SetLimits(idx, lo, lo + span);
            else
                upar.SetLimits(idx, hi - span, hi);
        }
        if (model.is_user_fixed(idx))
            upar.Fix(idx);
    }
    return upar;
}

// Minuit2 approaches an active limit asymptotically through its parameter
// transformation, so "on a limit" is decided with a small relative margin.
template <typename Model>
bool on_limit(const FitModel<Model> &model, unsigned int idx, double value) {
    const double margin = 1e-6 * std::max(1.0, std::abs(value));
    return std::abs(value - model.lower_limit(idx)) <= margin ||
           std::abs(value - model.upper_limit(idx)) <= margin;
}

// Copy the minimum into the flat result. Fixed parameters and parameters on
// a limit report an error of 0.
template <typename Model>
void unpack_minimum(const FitModel<Model> &model,
                    const ROOT::Minuit2::FunctionMinimum &min, double *par_out,
                    double *err_out, double &chi2) {
    constexpr std::size_t npar = Model::npar;
    const auto &state = min.UserState();
    const std::vector<double> values = state.Params();
    for (std::size_t k = 0; k < npar; ++k)
        par_out[k] = values[k];

    if (err_out) {
        const std::vector<double> errors = state.Errors();
        for (std::size_t k = 0; k < npar; ++k) {
            const auto idx = static_cast<unsigned int>(k);
            const bool no_error =
                model.is_user_fixed(idx) || on_limit(model, idx, values[k]);
            err_out[k] = no_error ? 0.0 : errors[k];
        }
    }
    chi2 = min.Fval();
}

} // namespace

template <typename Model>
bool fit_pixel_minuit2(const FitModel<Model> &model, NDView<double, 1> x,
                       NDView<double, 1> y, NDView<double, 1> y_err,
                       const std::array<double, Model::npar> &start,
                       double *par_out, double *err_out, double &chi2) {
    constexpr std::size_t npar = Model::npar;
    std::fill(par_out, par_out + npar, 0.0);
    if (err_out)
        std::fill(err_out, err_out + npar, 0.0);
    chi2 = 0.0;

    if (!Model::is_valid(std::vector<double>(start.begin(), start.end())))
        return false;

    const auto upar = user_parameters(model, x, y, start);
    const ROOT::Minuit2::MnStrategy strategy(model.strategy());

    if (model.minimizer() == Minimizer::Fumili) {
        // Fumili takes the gradient and the Gauss-Newton Hessian from the FCN,
        // so the covariance of the minimum is available without MnHesse.
        func::Chi2Model1DFumili<Model> fcn(x, y, y_err);
        ROOT::Minuit2::MnFumiliMinimize fumili(
            fcn, ROOT::Minuit2::MnUserParameterState(upar), strategy);
        const ROOT::Minuit2::FunctionMinimum min =
            fumili(model.max_calls(), model.tolerance());
        if (min.IsValid()) {
            unpack_minimum(model, min, par_out, err_out, chi2);
            return true;
        }
        // Minuit2's Fumili cannot invert its linearised Hessian once a
        // two-sided limit becomes active. Refit such pixels with Migrad.
    }

    func::Chi2Model1DGrad<Model> fcn(x, y, y_err);
    ROOT::Minuit2::MnMigrad migrad(fcn, upar, strategy);
    ROOT::Minuit2::FunctionMinimum min =
        migrad(model.max_calls(), model.tolerance());
    if (!min.IsValid())
        return false;

    // MnHesse cannot handle a fit in which every parameter is fixed.
    if (err_out && upar.VariableParameters() > 0) {
        ROOT::Minuit2::MnHesse hesse;
        hesse(fcn, min);
    }
    unpack_minimum(model, min, par_out, err_out, chi2);
    return true;
}

// NOLINTBEGIN
#define AARE_INSTANTIATE_MINUIT2_FIT(Model)                                    \
    template bool fit_pixel_minuit2<Model>(                                    \
        const FitModel<Model> &, NDView<double, 1>, NDView<double, 1>,         \
        NDView<double, 1>, const std::array<double, Model::npar> &, double *,  \
        double *, double &);

AARE_INSTANTIATE_MINUIT2_FIT(model::Gaussian)
AARE_INSTANTIATE_MINUIT2_FIT(model::GaussianErfcPlateau)
AARE_INSTANTIATE_MINUIT2_FIT(model::GaussianChargeSharing)
AARE_INSTANTIATE_MINUIT2_FIT(model::GaussianChargeSharingKb)
AARE_INSTANTIATE_MINUIT2_FIT(model::Pol1)
AARE_INSTANTIATE_MINUIT2_FIT(model::Pol2)
AARE_INSTANTIATE_MINUIT2_FIT(model::RisingScurve)
AARE_INSTANTIATE_MINUIT2_FIT(model::FallingScurve)

#undef AARE_INSTANTIATE_MINUIT2_FIT
// NOLINTEND

} // namespace aare::detail
