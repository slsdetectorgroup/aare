// SPDX-License-Identifier: MPL-2.0
#pragma once

#include "DampedGaussNewton.hpp"
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
 * only wastes a single pass. As in Minuit2, max_calls = 0 selects the
 * default budget 200 + 100 * npar + 5 * npar^2.
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

    using Result = MinimizerResult;
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
     * @param par_out Receives npar fitted values; zeros on failure.
     * @param err_out Receives npar errors when non-null; zeros on failure.
     */
    Result fit(const FitModel<Model> &model, NDView<double, 1> x,
               NDView<double, 1> y, NDView<double, 1> s,
               const std::array<double, Model::npar> &start, double *par_out,
               double *err_out) {
        x_ = x;
        y_ = y;
        s_ = s;
        weighted_ = s.size() > 0;
        const auto n = static_cast<std::size_t>(x.size());
        std::array<double, Model::npar> lower{};
        std::array<double, Model::npar> upper{};
        std::array<bool, Model::npar> fixed{};
        for (int k = 0; k < npar; ++k) {
            const auto idx = static_cast<unsigned int>(k);
            lower[k] = model.lower_limit(idx);
            upper[k] = model.upper_limit(idx);
            fixed[k] = model.is_user_fixed(idx);
        }
        r_.resize(n);
        J_.resize(n * static_cast<std::size_t>(npar));
        if (weighted_) {
            // 1 / s_i once per pixel rather than once per evaluation.
            w_.resize(n);
            for (std::size_t i = 0; i < n; ++i)
                w_[i] = s_[static_cast<ssize_t>(i)] != 0.0
                            ? 1.0 / s_[static_cast<ssize_t>(i)]
                            : 0.0;
        }
        const int max_calls = model.max_calls() > 0
                                  ? static_cast<int>(model.max_calls())
                                  : 200 + 100 * npar + 5 * npar * npar;

        auto eval = [this](const double *p, Normal &out) {
            return evaluate(p, out);
        };
        typename Driver::Options opt;
        opt.tolerance = model.tolerance();
        opt.max_calls = max_calls;
        // Start cautiously: the estimated start values of the linear
        // parameters can be far off, and the first steps are then wild.
        opt.damping0 = 0.1;
        const Result res = driver_.fit(eval, start.data(), lower.data(),
                                       upper.data(), fixed.data(), opt);
        if (!res.valid) {
            std::fill(par_out, par_out + npar, 0.0);
            if (err_out)
                std::fill(err_out, err_out + npar, 0.0);
            return res;
        }
        std::copy(driver_.point(), driver_.point() + npar, par_out);
        if (err_out)
            driver_.errors(err_out);
        return res;
    }

  private:
    double weight(ssize_t i) const {
        return weighted_ ? w_[static_cast<std::size_t>(i)] : 1.0;
    }

    // Residuals r = (y - f) / s and the Jacobian J = dr/dp at the point p,
    // reduced to F = chi2 / 2, g = J^T r and the upper triangle of J^T J.
    // The sums run over all parameters with fixed-size loops, so the
    // compiler can unroll them and keep the accumulators in registers.
    // Returns false when the model rejects the parameters.
    bool evaluate(const double *p, Normal &out) {
        pvec_.assign(p, p + npar);
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
        double G[npar] = {};
        double H[npar][npar] = {};
        for (ssize_t i = 0; i < n; ++i) {
            const double *Ji = &J_[static_cast<std::size_t>(i) * npar];
            const double ri = r_[i];
            for (int a = 0; a < npar; ++a) {
                G[a] += Ji[a] * ri;
                for (int b = a; b < npar; ++b)
                    H[a][b] += Ji[a] * Ji[b];
            }
        }
        out.F = F;
        for (int a = 0; a < npar; ++a) {
            out.g[a] = G[a];
            for (int b = a; b < npar; ++b)
                out.H[a][b] = H[a][b];
        }
        return true;
    }

    NDView<double, 1> x_, y_, s_;
    bool weighted_ = false;
    Driver driver_;
    std::vector<double> r_;
    std::vector<double> J_;
    std::vector<double> w_; // 1 / s_i for weighted fits
    std::vector<double> pvec_;
};

} // namespace aare::detail
