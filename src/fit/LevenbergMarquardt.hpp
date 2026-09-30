// SPDX-License-Identifier: MPL-2.0
#pragma once

#include "DampedGaussNewton.hpp"
#include "FitHelpers.hpp"
#include "aare/FitModel.hpp"
#include "aare/NDView.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <vector>

namespace aare::detail {

/**
 * @brief Levenberg-Marquardt least-squares minimiser for one FitModel.
 *
 * Minimises chi2 = sum_i ((y_i - f(x_i; p)) / s_i)^2 using the analytic
 * gradient of the model (s_i = 1 for unweighted fits; points with s_i == 0
 * are ignored). Keep one instance per thread: the buffers are reused between
 * pixels so that after the first fit no memory is allocated.
 *
 * Every evaluation computes the residuals r = (y - f) / s and the Jacobian
 * J = dr/dp in one pass over the data and reduces them to the normal
 * equations g = J^T r and J^T J, which DampedGaussNewton iterates on: it
 * handles fixed parameters, limits, damping and the convergence test, see
 * there. Rejected trials are rare with its damping rule, and a rejected one
 * only wastes a single pass. max_calls counts evaluations, see
 * evaluation_budget().
 *
 * Errors are Gauss-Newton estimates, sqrt(diag((J^T J)^-1)), over the free
 * parameters that do not sit on a limit. Fixed and limit-bound parameters
 * report 0.
 */
template <typename Model> class LevenbergMarquardt {
  public:
    static constexpr int npar = static_cast<int>(Model::npar);
    static_assert(Model::npar >= 1 && Model::npar <= 16,
                  "LevenbergMarquardt keeps npar x npar matrices on the stack");

    using Driver = DampedGaussNewton<npar>;
    using Normal = typename Driver::Normal;

    /**
     * @brief Fit one pixel.
     *
     * @param model   Limits, fixed flags and minimiser settings.
     * @param x       Scan points.
     * @param y       Measured values.
     * @param s       Per-point uncertainties, empty view for an unweighted fit.
     * @param start   Starting values, see start_values() in FitHelpers.hpp.
     * @param par_out Receives npar fitted values; untouched on failure.
     * @param err_out Receives npar errors when non-null; untouched on
     *                failure.
     */
    MinimizerResult fit(const FitModel<Model> &model, NDView<double, 1> x,
                        NDView<double, 1> y, NDView<double, 1> s,
                        const std::array<double, Model::npar> &start,
                        double *par_out, double *err_out) {
        x_ = x;
        y_ = y;
        weighted_ = s.size() > 0;
        const auto n = static_cast<std::size_t>(x.size());
        r_.resize(n);
        J_.resize(n * static_cast<std::size_t>(npar));
        if (weighted_) {
            // 1 / s_i once per pixel rather than once per evaluation.
            w_.resize(n);
            for (std::size_t i = 0; i < n; ++i) {
                const double si = s[static_cast<ssize_t>(i)];
                w_[i] = si != 0.0 ? 1.0 / si : 0.0;
            }
        }
        Vec<npar> lower{};
        Vec<npar> upper{};
        std::array<bool, Model::npar> fixed{};
        for (int k = 0; k < npar; ++k) {
            const auto idx = static_cast<unsigned int>(k);
            lower[k] = model.lower_limit(idx);
            upper[k] = model.upper_limit(idx);
            fixed[k] = model.is_user_fixed(idx);
        }
        typename Driver::Options opt;
        opt.tolerance = model.tolerance();
        opt.max_calls = evaluation_budget(model);

        auto eval = [this](const Vec<npar> &p, Normal &out) {
            return evaluate(p, out);
        };
        const auto res = driver_.fit(eval, start, lower, upper, fixed, opt);
        if (!res.valid)
            return res;
        std::copy(driver_.point().begin(), driver_.point().end(), par_out);
        if (err_out)
            driver_.errors(err_out);
        return res;
    }

  private:
    double weight(ssize_t i) const {
        return weighted_ ? w_[static_cast<std::size_t>(i)] : 1.0;
    }

    // Residuals r = (y - f) / s and the Jacobian J = dr/dp at the point p,
    // reduced to F = chi2 / 2, g = J^T r and J^T J.
    // The sums run over all parameters with fixed-size loops, so the
    // compiler can unroll them and keep the accumulators in registers.
    // Returns false when the model rejects the parameters.
    bool evaluate(const Vec<npar> &p, Normal &out) {
        pvec_.assign(p.begin(), p.end());
        if (!Model::is_valid(pvec_))
            return false;
        const ssize_t n = x_.size();
        double F = 0.0;
        std::array<double, Model::npar> g{};
        double f = 0.0;
        for (ssize_t i = 0; i < n; ++i) {
            const double w = weight(i);
            Model::eval_and_grad(x_[i], pvec_, f, g);
            r_[i] = (y_[i] - f) * w;
            F += 0.5 * r_[i] * r_[i];
            double *Ji = &J_[static_cast<std::size_t>(i) * npar];
            for (int k = 0; k < npar; ++k)
                Ji[k] = -g[k] * w;
        }
        Vec<npar> G{};
        SymMat<npar> H;
        for (ssize_t i = 0; i < n; ++i) {
            const double *Ji = &J_[static_cast<std::size_t>(i) * npar];
            const double ri = r_[i];
            for (int a = 0; a < npar; ++a)
                G[a] += Ji[a] * ri;
            H.rank1_update(Ji);
        }
        out.F = F;
        out.g = G;
        out.H = H;
        return true;
    }

    NDView<double, 1> x_, y_;
    bool weighted_ = false;
    Driver driver_;
    std::vector<double> r_;
    std::vector<double> J_;
    std::vector<double> w_; // 1 / s_i for weighted fits
    std::vector<double> pvec_;
};

} // namespace aare::detail
