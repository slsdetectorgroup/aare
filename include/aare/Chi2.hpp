// SPDX-License-Identifier: MPL-2.0
#pragma once
#include <algorithm>
#include <cmath>
#include <vector>
#include <Minuit2/FCNBase.h>
#include <Minuit2/FCNGradientBase.h>
#include "aare/NDView.hpp"
#include "aare/Models.hpp"

namespace aare {

namespace func {

/**
 * @brief Generic chi-squared FCN for a 1D model with optional per-point
 *        uncertainties.  Uses finite-difference gradients (Minuit internal).
 *
 * @tparam Model  A model struct that satisfies:
 *   - static constexpr std::size_t npar;
 *   - static double eval(double x, const std::vector<double>& par);
 *   - static bool is_valid(const std::vector<double>& par);
 *
 * Chi-squared definition:
 *   weighted   : sum_i ((y_i - f(x_i; par))^2 / sigma_i^2)
 *   unweighted : sum_i  (y_i - f(x_i; par))^2
 *
 * Data points with sigma_i == 0 are skipped to avoid division by zero.
 */
template <class Model>
class Chi2Model1D : public ROOT::Minuit2::FCNBase {
public:
    Chi2Model1D(NDView<double, 1> x,
                NDView<double, 1> y)
        : x_(x), y_(y), s_(), weighted_(false) {}

    Chi2Model1D(NDView<double, 1> x,
                NDView<double, 1> y,
                NDView<double, 1> y_err)
        : x_(x), y_(y), s_(y_err), weighted_(true) {}

    ~Chi2Model1D() override = default;

    double operator()(const std::vector<double>& par) const override {
        // Minimal sanity checks on width-like parameter for the Gaussian and the S-curve.
        if (par.size() != Model::npar) return 1e20;
        if (!Model::is_valid(par)) return 1e20;

        double chi2 = 0.0;

        if (weighted_) {
            for (ssize_t i = 0; i < x_.size(); ++i) {
                if (s_[i] == 0.0) continue;
                const double f_i = Model::eval(x_[i], par);
                const double r_i = y_[i] - f_i;
                chi2 += r_i * r_i / (s_[i] * s_[i]);
            }
        } else {
            for (ssize_t i = 0; i < x_.size(); ++i) {
                const double f_i = Model::eval(x_[i], par);
                const double r_i = y_[i] - f_i;
                chi2 += r_i * r_i;
            }
        }

        return chi2;
    }
    
    /** @brief Error definition: 1.0 for chi-squared (delta_chi2 = 1 -> 1-sigma). */
    double Up() const override { return 1.0; }

private:
    NDView<double, 1> x_;
    NDView<double, 1> y_;
    NDView<double, 1> s_;
    bool weighted_;
};

/**
 * @brief Generic chi-squared FCN with analytic gradient for a 1D model.
 *
 * @tparam Model  A model struct that satisfies:
 *   - static constexpr std::size_t npar;
 *   - static double eval(double x, const std::vector<double>& par);
 *   - static void eval_and_grad(double x, const std::vector<double>& par,
 *                                double& f, std::array<double, npar>& g);
 *   - static bool is_valid(const std::vector<double>& par);
 *
 * Gradient:
 *   d(chi2)/dp_k = -2 * sum_i  w_i * (y_i - f_i) * df_i/dp_k
 *
 *   where w_i = 1/sigma_i^2 (weighted) or 1 (unweighted).
 *
 * By providing analytic gradients we avoid 2*npar extra function evaluations
 * per Minuit step that would otherwise be spent on finite differences.
 */
template <class Model>
class Chi2Model1DGrad : public ROOT::Minuit2::FCNGradientBase {
public:
    Chi2Model1DGrad(NDView<double, 1> x,
                    NDView<double, 1> y)
        : x_(x), y_(y), s_(), weighted_(false) {}

    Chi2Model1DGrad(NDView<double, 1> x,
                    NDView<double, 1> y,
                    NDView<double, 1> y_err)
        : x_(x), y_(y), s_(y_err), weighted_(true) {}

    ~Chi2Model1DGrad() override = default;

    double operator()(const std::vector<double>& par) const override {
        if (par.size() != Model::npar) return 1e20;
        if (!Model::is_valid(par)) return 1e20;

        double chi2 = 0.0;

        if (weighted_) {
            for (ssize_t i = 0; i < x_.size(); ++i) {
                const double si = s_[i];
                if (si == 0.0) continue;

                const double f_i = Model::eval(x_[i], par);
                const double r_i = y_[i] - f_i;
                chi2 += (r_i * r_i) / (si * si);
            }
        } else {
            for (ssize_t i = 0; i < x_.size(); ++i) {
                const double f_i = Model::eval(x_[i], par);
                const double r_i = y_[i] - f_i;
                chi2 += r_i * r_i;
            }
        }

        return chi2;
    }

    std::vector<double> Gradient(const std::vector<double>& par) const override {
        std::vector<double> grad(Model::npar, 0.0);

        if (par.size() != Model::npar) return grad;
        if (!Model::is_valid(par)) return grad;

        std::array<double, Model::npar> df{};
        double f_i = 0.0;

        if (weighted_) {
            for (ssize_t i = 0; i < x_.size(); ++i) {
                const double si = s_[i];
                if (si == 0.0) continue;

                Model::eval_and_grad(x_[i], par, f_i, df);

                const double r_i = y_[i] - f_i;
                const double c   = -2.0 * r_i / (si * si);

                for (std::size_t k = 0; k < Model::npar; ++k) {
                    grad[k] += c * df[k];
                }
            }
        } else {
            for (ssize_t i = 0; i < x_.size(); ++i) {
                Model::eval_and_grad(x_[i], par, f_i, df);

                const double r_i = y_[i] - f_i;
                const double c   = -2.0 * r_i;

                for (std::size_t k = 0; k < Model::npar; ++k) {
                    grad[k] += c * df[k];
                }
            }
        }

        return grad;
    }

    /** @brief Error definition: 1.0 for chi-squared (delta_chi2 = 1 -> 1-sigma). */
    double Up() const override { return 1.0; }

private:
    NDView<double, 1> x_;
    NDView<double, 1> y_;
    NDView<double, 1> s_;
    bool weighted_;
};

// ── Convenient aliases ──────────────────────────────────────────────

using Chi2Gaussian = Chi2Model1D<aare::model::Gaussian>;
using Chi2SCurve   = Chi2Model1D<aare::model::SCurveRising>;
using Chi2SCurve2  = Chi2Model1D<aare::model::SCurveFalling>;

using Chi2GaussianGradient = Chi2Model1DGrad<aare::model::Gaussian>;
using Chi2SCurveGradient   = Chi2Model1DGrad<aare::model::SCurveRising>;
using Chi2SCurve2Gradient  = Chi2Model1DGrad<aare::model::SCurveFalling>;

} // namespace aare::func

} // aare