// SPDX-License-Identifier: MPL-2.0
#pragma once
#include <algorithm>
#include <cmath>
#include <vector>
#include <Minuit2/FCNBase.h>
#include "aare/NDView.hpp"
#include "aare/SCurves.hpp"

namespace aare {

namespace func {

/**
 * @brief Generic chi-squared FCN for a 1D model with optional per-point uncertainties.
 *
 * Template parameter Model must provide:
 *
 *   static constexpr std::size_t npar;
 *   static double eval(double x, const std::vector<double>& par);
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
        // Minimal sanity checks on width-like parameter for the S-curve.
        // For the 6-par curve  -> par[3] = width
        if (par.size() != Model::npar) return 1e20;
        if (par[3] == 0.0) return 1e20;

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

    double Up() const override { return 1.0; }

private:
    NDView<double, 1> x_;
    NDView<double, 1> y_;
    NDView<double, 1> s_;
    bool weighted_;
};

// Convenient aliases
using Chi2SCurve        = Chi2Model1D<aare::model::SCurveRising>;
using Chi2SCurve2       = Chi2Model1D<aare::model::SCurveFalling>;

} // namespace aare::func

} // aare