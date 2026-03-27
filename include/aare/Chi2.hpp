// SPDX-License-Identifier: MPL-2.0
#pragma once
#include <stdexcept>
#include <algorithm>
#include <cmath>
#include <vector>
#include <Minuit2/FCNGradientBase.h>
#include "aare/NDView.hpp"
#include "aare/Models.hpp"

namespace aare {

namespace func {

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
 * 
 * @throws std::invalid_argument if par.size() != Model::npar.
 *
 * Invalid model parameters do not throw; they return a large penalty
 * (and a zero gradient fallback) so the minimizer can remain in control.
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
        if (par.size() != Model::npar) {
            throw std::invalid_argument("Chi2Model1DGrad: wrong parameter vector size.");
        }
        
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
        if (par.size() != Model::npar) {
            throw std::invalid_argument("Chi2Model1DGrad: wrong parameter vector size.");
        }
        
        std::vector<double> grad(Model::npar, 0.0);
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

using Chi2Gaussian      = Chi2Model1DGrad<aare::model::Gaussian>;
using Chi2RisingScurve  = Chi2Model1DGrad<aare::model::RisingScurve>;
using Chi2FallingScurve = Chi2Model1DGrad<aare::model::FallingScurve>;
using Chi2Pol1    = Chi2Model1DGrad<aare::model::Pol1>;
using Chi2Pol2    = Chi2Model1DGrad<aare::model::Pol2>;

} // namespace aare::func

} // aare