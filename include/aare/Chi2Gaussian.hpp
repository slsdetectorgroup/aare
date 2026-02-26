// SPDX-License-Identifier: MPL-2.0
#pragma once
#include "aare/NDArray.hpp"
#include "aare/NDView.hpp"

#include <Minuit2/FCNBase.h>
#include <cmath>
#include <vector>

namespace aare::func {

/**
 * @brief Chi-squared functor for fitting an unnormalized Gaussian
 *        to 1D data with per-point uncertainties.
 *
 * Model:  f(x) = A * exp( -(x - mu)^2 / (2 * sigma^2) )
 *
 * Parameters (indexed in the vector passed by Minuit):
 *   par[0] = A     (amplitude)
 *   par[1] = mu    (mean)
 *   par[2] = sigma (standard deviation)
 *
 * Implements ROOT::Minuit2::FCNBase.  Minuit estimates the gradient
 * via finite differences — expect ~7 function evaluations per step
 * (1 central + 2 per parameter for central differences).
 */
class Chi2Gaussian : public ROOT::Minuit2::FCNBase {
public:
    /**
     * @brief Construct the chi-squared functor.
     *
     * @param x     Scan points (energies gor e.g.).
     * @param y     Measured values at each scan point.
     * @param y_err Per-point uncertainties (standard deviations, not variances).
     *
     * All three views must have the same size and must outlive this object.
     */
    Chi2Gaussian(NDView<double, 1> x,
            NDView<double, 1> y)
        : x_(x), y_(y), s_(), weighted_(false) {}

    Chi2Gaussian(NDView<double, 1> x,
                 NDView<double, 1> y,
                 NDView<double, 1> y_err)
        : x_(x), y_(y), s_(y_err), weighted_(true) {}

    ~Chi2Gaussian() override = default;

    /**
     * @brief Evaluate chi-squared at the given parameter point.
     *
     * Points where the uncertainty is zero are skipped to avoid
     * division by zero.  A degenerate sigma (== 0) returns a large
     * penalty value so the optimizer walks away.
     */
    double operator()(const std::vector<double> &par) const override {
        const double A   = par[0];
        const double mu  = par[1];
        const double sig = par[2];

        if (sig == 0.0) return 1e20;

        const double inv_2sig2 = 1.0 / (2.0 * sig * sig);

        double chi2 = 0.0;

        // ugly but allows vectorization
        if (weighted_) {
            for (ssize_t i = 0; i < x_.size(); ++i) {
                if (s_[i] == 0.0) continue;
                double dx  = x_[i] - mu;
                double g_i = A * std::exp(-dx * dx * inv_2sig2);
                double r_i = y_[i] - g_i;
                chi2 += r_i * r_i / (s_[i] * s_[i]);
            }
        } else {
            for (ssize_t i = 0; i < x_.size(); ++i) {
                double dx  = x_[i] - mu;
                double g_i = A * std::exp(-dx * dx * inv_2sig2);
                double r_i = y_[i] - g_i;
                chi2 += r_i * r_i;
            }
        }
        return chi2;
    }

    /**
     * @brief Error definition for Minuit.
     *
     * Returns 1.0 because this is a chi-squared (not a negative
     * log-likelihood, which would return 0.5).  This value tells
     * Minuit that delta_chi2 = 1 corresponds to a 1-sigma error
     * on a single parameter.
     */
    double Up() const override { return 1.0; }

private:
    NDView<double, 1> x_;
    NDView<double, 1> y_;
    NDView<double, 1> s_;
    bool weighted_;
};

} // namespace aare::func