// SPDX-License-Identifier: MPL-2.0
#pragma once

#include <array>
#include <cmath>
#include <limits>
#include <vector>
#include <Minuit2/FCNGradientBase.h>
#include "aare/NDView.hpp"
#include "aare/SCurves.hpp"


namespace aare {

namespace func {

/**
 * @brief Generic chi-squared FCN with analytic gradient for a 1D model.
 *
 * Template parameter Model must provide:
 *
 *   static constexpr std::size_t npar;
 *   static double eval(double x, const std::vector<double>& par);
 *   static void eval_and_grad(double x,
 *                             const std::vector<double>& par,
 *                             double& f,
 *                             std::array<double, npar>& g);
 *
 * Chi2:
 *   weighted   : sum_i ((y_i - f_i)^2 / s_i^2)
 *   unweighted : sum_i (y_i - f_i)^2
 *
 * Gradient:
 *   d chi2 / d p_k = -2 sum_i [ (y_i - f_i) / s_i^2 ] * d f_i / d p_k
 *   or without weights:
 *   d chi2 / d p_k = -2 sum_i [ (y_i - f_i) ] * d f_i / d p_k
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
        if (std::abs(par[3]) < 1e-15) return 1e20;

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
        if (std::abs(par[3]) < 1e-15) return grad;

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

    double Up() const override { return 1.0; }

private:
    NDView<double, 1> x_;
    NDView<double, 1> y_;
    NDView<double, 1> s_;
    bool weighted_;
};

// Convenient aliases
using Chi2SCurveGrad  = Chi2Model1DGrad<aare::model::SCurveRising>;
using Chi2SCurve2Grad = Chi2Model1DGrad<aare::model::SCurveFalling>;

} // namespace func

} // namespace aare