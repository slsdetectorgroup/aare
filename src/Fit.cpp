// SPDX-License-Identifier: MPL-2.0
#include "aare/Fit.hpp"
#include "FitHelpers.hpp"
#include "FitMinuit2.hpp"
#include "LevenbergMarquardt.hpp"
#include "VariableProjection.hpp"
#include "aare/Models.hpp"
#include "aare/utils/par.hpp"
#include "aare/utils/task.hpp"
#include <array>
#include <cstddef>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <vector>

namespace aare {
// ============================================================================
// FitModel<Model> — method definitions
// ============================================================================

template <typename Model>
FitModel<Model>::FitModel(unsigned int strategy, unsigned int max_calls,
                          double tolerance, bool compute_errors,
                          Minimizer minimizer)
    : strategy_(strategy), max_calls_(max_calls), tolerance_(tolerance),
      compute_errors_(compute_errors), minimizer_(minimizer) {
    for (std::size_t i = 0; i < npar; ++i) {
        lower_[i] = Model::param_info[i].default_lo;
        upper_[i] = Model::param_info[i].default_hi;
    }
}

template <typename Model>
void FitModel<Model>::check_index(unsigned int idx) const {
    if (idx >= npar)
        throw std::out_of_range("FitModel: parameter index " +
                                std::to_string(idx) + " out of range");
}

template <typename Model>
unsigned int FitModel<Model>::checked_index(const std::string &name) const {
    for (std::size_t i = 0; i < npar; ++i) {
        if (name == Model::param_info[i].name)
            return static_cast<unsigned int>(i);
    }
    throw std::runtime_error("FitModel: unknown parameter name '" + name + "'");
}

template <typename Model>
void FitModel<Model>::SetParLimits(unsigned int idx, double lo, double hi) {
    check_index(idx);
    if (!(lo < hi))
        throw std::runtime_error(
            "FitModel: lower limit must be below upper limit for parameter '" +
            GetParName(idx) + "'");
    lower_[idx] = lo;
    upper_[idx] = hi;
}

template <typename Model>
void FitModel<Model>::FixParameter(unsigned int idx, double val) {
    SetParameter(idx, val);
    user_fixed_[idx] = true;
}

template <typename Model>
void FitModel<Model>::ReleaseParameter(unsigned int idx) {
    check_index(idx);
    user_fixed_[idx] = false;
}

template <typename Model>
void FitModel<Model>::ReleaseParameter(const std::string &name) {
    ReleaseParameter(checked_index(name));
}

template <typename Model>
void FitModel<Model>::SetParameter(unsigned int idx, double val) {
    check_index(idx);
    value_[idx] = val;
    user_start_[idx] = true;
}

template <typename Model>
void FitModel<Model>::SetParameter(const std::string &name, double val) {
    SetParameter(checked_index(name), val);
}

template <typename Model>
void FitModel<Model>::FixParameter(const std::string &name, double val) {
    FixParameter(checked_index(name), val);
}

template <typename Model>
void FitModel<Model>::SetParLimits(const std::string &name, double lo,
                                   double hi) {
    SetParLimits(checked_index(name), lo, hi);
}

template <typename Model>
std::string FitModel<Model>::GetParName(unsigned int idx) const {
    check_index(idx);
    return Model::param_info[idx].name;
}

template <typename Model>
std::vector<std::string> FitModel<Model>::GetParNames() const {
    std::vector<std::string> names;
    names.reserve(npar);
    for (std::size_t i = 0; i < npar; ++i)
        names.emplace_back(Model::param_info[i].name);
    return names;
}

// ============================================================================
// fit_pixel / fit_3d
// ============================================================================

namespace detail {

struct NotSeparable {};

/**
 * @brief Fits pixels with the minimizer selected by the model.
 *
 * Keep one instance per thread: the buffers of the built-in minimizers are
 * reused between pixels so that a data cube is fitted without per-pixel
 * allocations. The Minuit2 minimizers create their state per pixel.
 */
template <typename Model> class PixelFitter {
  public:
    /**
     * @brief Fit one pixel.
     *
     * @param par_out Receives npar fitted values; zeros on failure.
     * @param err_out Receives npar errors when non-null; zeros on failure.
     * @param chi2    Receives the chi-squared at the minimum; 0 on failure.
     * @return true for a valid minimum.
     */
    bool fit(const FitModel<Model> &model, NDView<double, 1> x,
             NDView<double, 1> y, NDView<double, 1> y_err, double *par_out,
             double *err_out, double &chi2) {
        const auto start = start_values(model, x, y);

        if (model.minimizer() == Minimizer::VarPro) {
            // A model without the separable structure, a pixel that does
            // not converge and a pixel whose linear solution violates a
            // limit fall back to the full solver.
            if constexpr (model::is_separable<Model>::value) {
                const auto res =
                    vp_.fit(model, x, y, y_err, start, par_out, err_out);
                if (res.valid) {
                    chi2 = res.chi2;
                    return true;
                }
            }
            return fit_lm(model, x, y, y_err, start, par_out, err_out, chi2);
        }
        if (model.minimizer() == Minimizer::LevenbergMarquardt)
            return fit_lm(model, x, y, y_err, start, par_out, err_out, chi2);
        return fit_pixel_minuit2(model, x, y, y_err, start, par_out, err_out,
                                 chi2);
    }

  private:
    bool fit_lm(const FitModel<Model> &model, NDView<double, 1> x,
                NDView<double, 1> y, NDView<double, 1> y_err,
                const std::array<double, Model::npar> &start, double *par_out,
                double *err_out, double &chi2) {
        const auto res = lm_.fit(model, x, y, y_err, start, par_out, err_out);
        chi2 = res.valid ? res.chi2 : 0.0;
        return res.valid;
    }

    LevenbergMarquardt<Model> lm_;
    std::conditional_t<model::is_separable<Model>::value,
                       VariableProjection<Model>, NotSeparable>
        vp_;
};

} // namespace detail

template <typename Model>
NDArray<double, 1> fit_pixel(const FitModel<Model> &model, NDView<double, 1> x,
                             NDView<double, 1> y, NDView<double, 1> y_err) {
    constexpr std::size_t npar = Model::npar;
    const bool want_errors = model.compute_errors();
    const auto result_size =
        static_cast<ssize_t>(want_errors ? (2 * npar + 1) : (npar + 1));
    NDArray<double, 1> result({result_size}, 0.0);

    detail::PixelFitter<Model> fitter;
    double chi2 = 0.0;
    double *par = result.data();
    double *err = want_errors ? result.data() + npar : nullptr;
    if (!fitter.fit(model, x, y, y_err, par, err, chi2))
        return result; // all zeros

    result(result_size - 1) = chi2;
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
        detail::PixelFitter<Model> fitter; // buffers reused within the task
        std::array<double, npar> par{};
        std::array<double, npar> err{};
        for (ssize_t row = first_row; row < last_row; row++) {
            for (ssize_t col = 0; col < y.shape(1); col++) {
                NDView<double, 1> values(&y(row, col, 0), {y.shape(2)});
                NDView<double, 1> errors =
                    has_errors ? NDView<double, 1>(&y_err(row, col, 0),
                                                   {y_err.shape(2)})
                               : NDView<double, 1>{};

                double chi2 = 0.0;
                fitter.fit(model, x, values, errors, par.data(),
                           want_par_errors ? err.data() : nullptr, chi2);

                for (std::size_t k = 0; k < npar; ++k)
                    par_out(row, col, static_cast<ssize_t>(k)) = par[k];
                if (want_par_errors) {
                    for (std::size_t k = 0; k < npar; ++k)
                        err_out(row, col, static_cast<ssize_t>(k)) = err[k];
                }
                chi2_out(row, col) = chi2;
            }
        }
    };

    auto tasks = split_task(0, static_cast<int>(y.shape(0)), n_threads);
    RunInParallel(process, tasks);
}

// ============================================================================
// Explicit instantiations for all supported model types
// ============================================================================

// NOLINTBEGIN
#define AARE_INSTANTIATE_FIT(Model)                                            \
    template class FitModel<Model>;                                            \
    template NDArray<double, 1> fit_pixel<Model>(                              \
        const FitModel<Model> &, NDView<double, 1>, NDView<double, 1>,         \
        NDView<double, 1>);                                                    \
    template NDArray<double, 1> fit_pixel<Model>(                              \
        const FitModel<Model> &, NDView<double, 1>, NDView<double, 1>);        \
    template void fit_3d<Model>(const FitModel<Model> &, NDView<double, 1>,    \
                                NDView<double, 3>, NDView<double, 3>,          \
                                NDView<double, 3>, NDView<double, 3>,          \
                                NDView<double, 2>, int);

AARE_INSTANTIATE_FIT(model::Gaussian)
AARE_INSTANTIATE_FIT(model::GaussianErfcPlateau)
AARE_INSTANTIATE_FIT(model::GaussianChargeSharing)
AARE_INSTANTIATE_FIT(model::GaussianChargeSharingKb)
AARE_INSTANTIATE_FIT(model::Pol1)
AARE_INSTANTIATE_FIT(model::Pol2)
AARE_INSTANTIATE_FIT(model::RisingScurve)
AARE_INSTANTIATE_FIT(model::FallingScurve)

#undef AARE_INSTANTIATE_FIT
// NOLINTEND

} // namespace aare
