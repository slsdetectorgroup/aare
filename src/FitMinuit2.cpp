// SPDX-License-Identifier: MPL-2.0
// Optional Minuit2 (Migrad + Hesse) fitting backend, compiled only with
// -DAARE_MINUIT2=ON. Kept for side-by-side validation of the built-in
// Levenberg-Marquardt solver in Fit.cpp.
#include "aare/FitMinuit2.hpp"
#include "Chi2.hpp"
#include "FitHelpers.hpp"
#include "Minuit2/FunctionMinimum.h"
#include "Minuit2/MnHesse.h"
#include "Minuit2/MnMigrad.h"
#include "Minuit2/MnStrategy.h"
#include "Minuit2/MnUserParameters.h"
#include "MinuitSteps.hpp"
#include "aare/Models.hpp"
#include "aare/utils/par.hpp"
#include "aare/utils/task.hpp"
#include <array>
#include <cmath>
#include <vector>

namespace aare::minuit2 {

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
    compute_ranges(x, y, x_range, y_range, slope_scale);
    std::array<double, npar> steps{};
    Steps<Model>::compute(start, x_range, y_range, slope_scale, steps);

    ROOT::Minuit2::MnUserParameters upar;
    for (std::size_t i = 0; i < npar; ++i) {
        const auto idx = static_cast<unsigned int>(i);
        upar.Add(Model::param_info[i].name, start[i], steps[i]);
        const double lo = model.lower_limit(idx);
        const double hi = model.upper_limit(idx);
        if (std::isfinite(lo) && std::isfinite(hi))
            upar.SetLimits(idx, lo, hi);
        else if (std::isfinite(lo))
            upar.SetLowerLimit(idx, lo);
        else if (std::isfinite(hi))
            upar.SetUpperLimit(idx, hi);
        if (model.is_user_fixed(idx))
            upar.Fix(idx);
    }
    return upar;
}

} // namespace

template <typename Model>
NDArray<double, 1> fit_pixel(const FitModel<Model> &model, NDView<double, 1> x,
                             NDView<double, 1> y, NDView<double, 1> y_err) {
    using FCN = func::Chi2Model1DGrad<Model>;
    constexpr std::size_t npar = Model::npar;
    const bool want_errors = model.compute_errors();
    const auto result_size =
        static_cast<ssize_t>(want_errors ? (2 * npar + 1) : (npar + 1));
    NDArray<double, 1> result({result_size}, 0.0);

    const auto start = detail::start_values(model, x, y);
    if (!Model::is_valid(std::vector<double>(start.begin(), start.end())))
        return result;

    const auto upar = user_parameters(model, x, y, start);
    auto chi2 = (y_err.size() > 0) ? FCN(x, y, y_err) : FCN(x, y);

    ROOT::Minuit2::MnMigrad migrad(chi2, upar,
                                   ROOT::Minuit2::MnStrategy(model.strategy()));
    ROOT::Minuit2::FunctionMinimum min =
        migrad(model.max_calls(), model.tolerance());

    if (!min.IsValid())
        return result;

    if (want_errors) {
        ROOT::Minuit2::MnHesse hesse;
        hesse(chi2, min);
    }

    const auto &values = min.UserState().Params();
    for (std::size_t k = 0; k < npar; ++k)
        result(static_cast<ssize_t>(k)) = values[k];

    if (want_errors) {
        const auto &errors = min.UserState().Errors();
        for (std::size_t k = 0; k < npar; ++k) {
            const auto idx = static_cast<unsigned int>(k);
            result(static_cast<ssize_t>(npar + k)) =
                model.is_user_fixed(idx) ? 0.0 : errors[k];
        }
        result(static_cast<ssize_t>(2 * npar)) = min.Fval();
    } else {
        result(static_cast<ssize_t>(npar)) = min.Fval();
    }
    return result;
}

template <typename Model>
NDArray<double, 1> fit_pixel(const FitModel<Model> &model, NDView<double, 1> x,
                             NDView<double, 1> y) {
    return fit_pixel(model, x, y, NDView<double, 1>{});
}

template <typename Model>
void fit_3d(const FitModel<Model> &model, NDView<double, 1> x,
            NDView<double, 3> y, NDView<double, 3> y_err,
            NDView<double, 3> par_out, NDView<double, 3> err_out,
            NDView<double, 2> chi2_out, int n_threads) {
    constexpr std::size_t npar = Model::npar;
    detail::check_fit_3d_shapes<Model>(x, y, y_err, par_out, err_out, chi2_out);

    const bool has_errors = (y_err.size() > 0);
    const bool want_par_errors = (err_out.size() > 0) && model.compute_errors();

    auto process = [&](ssize_t first_row, ssize_t last_row) {
        for (ssize_t row = first_row; row < last_row; row++) {
            for (ssize_t col = 0; col < y.shape(1); col++) {
                NDView<double, 1> values(&y(row, col, 0), {y.shape(2)});
                NDView<double, 1> errors =
                    has_errors ? NDView<double, 1>(&y_err(row, col, 0),
                                                   {y_err.shape(2)})
                               : NDView<double, 1>{};

                auto res = fit_pixel(model, x, values, errors);

                for (std::size_t k = 0; k < npar; ++k)
                    par_out(row, col, static_cast<ssize_t>(k)) =
                        res(static_cast<ssize_t>(k));

                if (want_par_errors) {
                    for (std::size_t k = 0; k < npar; ++k)
                        err_out(row, col, static_cast<ssize_t>(k)) =
                            res(static_cast<ssize_t>(npar + k));
                    chi2_out(row, col) = res(static_cast<ssize_t>(2 * npar));
                } else {
                    chi2_out(row, col) = res(static_cast<ssize_t>(npar));
                }
            }
        }
    };

    auto tasks = split_task(0, static_cast<int>(y.shape(0)), n_threads);
    RunInParallel(process, tasks);
}

// NOLINTBEGIN
#define AARE_INSTANTIATE_MINUIT2_FIT(Model)                                    \
    template NDArray<double, 1> fit_pixel<Model>(                              \
        const FitModel<Model> &, NDView<double, 1>, NDView<double, 1>,         \
        NDView<double, 1>);                                                    \
    template NDArray<double, 1> fit_pixel<Model>(                              \
        const FitModel<Model> &, NDView<double, 1>, NDView<double, 1>);        \
    template void fit_3d<Model>(const FitModel<Model> &, NDView<double, 1>,    \
                                NDView<double, 3>, NDView<double, 3>,          \
                                NDView<double, 3>, NDView<double, 3>,          \
                                NDView<double, 2>, int);

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

} // namespace aare::minuit2
