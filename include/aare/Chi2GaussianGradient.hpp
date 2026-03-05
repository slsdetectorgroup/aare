// SPDX-License-Identifier: MPL-2.0
#pragma once

#include "aare/NDArray.hpp"
#include "aare/NDView.hpp"

#include <Minuit2/FCNGradientBase.h>
#include <cmath>
#include <vector>

namespace aare::func {

/**
 * @brief Chi-squared functor with analytic gradient for fitting an
 *        unnormalized Gaussian to 1D data with per-point uncertainties.
 *
 * Model:  f(x) = A * exp( -(x - mu)^2 / (2 * sigma^2) )
 *
 * Parameters (indexed in the vector passed by Minuit):
 *   par[0] = A     (amplitude)
 *   par[1] = mu    (mean)
 *   par[2] = sigma (standard deviation)
 *
 * Analytic partial derivatives of chi-squared:
 *
 *   Let g_i = exp( -(x_i - mu)^2 / (2 sigma^2) )
 *       r_i = y_i - A * g_i
 *       w_i = 1 / sigma_err_i^2
 *
 *   d(chi2)/dA     = -2  SUM_i  w_i * r_i * g_i
 *   d(chi2)/d(mu)  = -2  SUM_i  w_i * r_i * A * g_i * (x_i - mu) / sigma^2
 *   d(chi2)/d(sig) = -2  SUM_i  w_i * r_i * A * g_i * (x_i - mu)^2 / sigma^3
 *
 * Implements ROOT::Minuit2::FCNGradientBase.  By providing analytic
 * gradients we avoid 2*N_par extra function evaluations per step that
 * Minuit would otherwise spend on finite differences.  A mutable cache
 * fuses the chi2 and gradient computation into a single pass over the
 * data so that the expensive exp() call happens only once per point
 * per parameter update.
 */
class Chi2GaussianGradient : public ROOT::Minuit2::FCNGradientBase {
public:
    /**
     * @brief Construct the chi-squared + gradient functor.
     *
     * @param x     Scan points.
     * @param y     Measured values at each scan point.
     * @param y_err Per-point uncertainties.
     *
     * All three views must have the same size and must outlive this object.
     */
    Chi2GaussianGradient(NDView<double, 1> x,
                        NDView<double, 1> y)
        : x_(x), y_(y), s_(), weighted_(false) {}
    
    Chi2GaussianGradient(NDView<double, 1> x,
                         NDView<double, 1> y,
                         NDView<double, 1> y_err)
        : x_(x), y_(y), s_(y_err), weighted_(true) {}

    ~Chi2GaussianGradient() override = default;

    /**
     * @brief Evaluate chi-squared at the given parameter point.
     *
     * Internally calls compute() which also produces the gradient.
     * If Minuit subsequently requests the gradient at the same point,
     * the cached result is returned without recomputation.
     */
    double operator()(const std::vector<double> &par) const override {
        compute(par);
        return cached_chi2_;
    }

    /**
     * @brief Return the analytic gradient of chi-squared.
     *
     * Internally calls compute() which also produces chi-squared.
     * If Minuit previously evaluated operator() at the same point,
     * the cached result is returned without recomputation.
     */
    std::vector<double> Gradient(const std::vector<double> &par) const override {
        compute(par);
        return cached_grad_;
    }

    /**
     * @brief Error definition for Minuit.
     *
     * Returns 1.0 for chi-squared minimization (delta_chi2 = 1
     * corresponds to a 1-sigma error on a single parameter).
     */
    double Up() const override { return 1.0; }

private:
    NDView<double, 1> x_;
    NDView<double, 1> y_;
    NDView<double, 1> s_;
    bool weighted_;

    // mutable cache for fused chi2 + gradient computation
    mutable std::vector<double> cached_par_;
    mutable double              cached_chi2_{0.0};
    mutable std::vector<double> cached_grad_{0.0, 0.0, 0.0};

    /**
     * @brief Fused computation of chi-squared and its gradient.
     *
     * Performs a single pass over the data arrays, evaluating exp()
     * once per point and accumulating both the chi-squared sum and
     * all three partial derivatives simultaneously.
     *
     * Results are stored in mutable members and re-used if Minuit
     * calls operator() and Gradient() at the same parameter point
     * (which is the typical calling pattern).
     *
     * Guard clauses:
     *  - Skips recomputation if par matches the cached parameters.
     *  - Returns a large penalty if sigma == 0 (degenerate model).
     *  - Skips data points where the uncertainty is zero.
     */
    void compute(const std::vector<double> &par) const {
        if (par == cached_par_)
            return;

        cached_par_ = par;

        const double A   = par[0];
        const double mu  = par[1];
        const double sig = par[2];

        if (sig == 0.0) {
            cached_chi2_ = 1e20;
            cached_grad_ = {0.0, 0.0, 0.0};
            return;
        }

        const double inv_sig2  = 1.0 / (sig * sig);
        const double inv_sig3  = inv_sig2 / sig;
        const double inv_2sig2 = 0.5 * inv_sig2;

        double chi2 = 0.0;
        double dA   = 0.0;
        double dMu  = 0.0;
        double dSig = 0.0;

        if (weighted_) {
            for (ssize_t i = 0; i < x_.size(); ++i) {
                if (s_[i] == 0.0) continue;
    
                const double dx  = x_[i] - mu;
                const double dx2 = dx * dx;
                const double g_i = exp(-dx2 * inv_2sig2);
                const double Ag  = A * g_i;
                const double r_i = y_[i] - Ag;
                const double w   = 1.0 / (s_[i] * s_[i]);
    
                chi2 += r_i * r_i * w;
    
                const double common = -2.0 * w * r_i * g_i;
                dA   += common;
                dMu  += common * A * dx  * inv_sig2;
                dSig += common * A * dx2 * inv_sig3;
            }
        } else {
            for (ssize_t i = 0; i < x_.size(); ++i) {
                const double dx  = x_[i] - mu;
                const double dx2 = dx * dx;
                const double g_i = exp(-dx2 * inv_2sig2);
                const double Ag  = A * g_i;
                const double r_i = y_[i] - Ag;
    
                chi2 += r_i * r_i;
    
                const double common = -2.0 * r_i * g_i;
                dA   += common;
                dMu  += common * A * dx  * inv_sig2;
                dSig += common * A * dx2 * inv_sig3;
            }
        }

        cached_chi2_ = chi2;
        cached_grad_ = {dA, dMu, dSig};
    }
};

} // namespace aare::func