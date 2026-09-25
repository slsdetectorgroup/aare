// SPDX-License-Identifier: MPL-2.0
#pragma once

#include "DampedGaussNewton.hpp"
#include "aare/FitModel.hpp"
#include "aare/Models.hpp"
#include "aare/NDView.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <vector>

namespace aare::detail {

/**
 * @brief Variable projection least-squares minimiser for a separable
 * FitModel (Golub and Pereyra, 1973).
 *
 * The model declares the parameters it is linear in, see
 * model::is_separable. For a given point of the nonlinear parameters the
 * linear ones are solved exactly from their normal equations, and the
 * damped Gauss-Newton iteration of DampedGaussNewton runs over the
 * nonlinear parameters only, on the projected residual
 * r(alpha) = y - Phi(alpha) beta(alpha). Its Jacobian uses Kaufman's
 * approximation, the derivatives of the model at fixed beta projected out
 * of the span of the basis functions, and the reduced Gauss-Newton Hessian
 * is the Schur complement of the full one. Compared with
 * LevenbergMarquardt the iteration needs fewer evaluations, cannot be misled
 * by poor start values of the linear parameters, and each evaluation
 * accumulates far fewer sums. The driver rejects trial points at which the
 * model loses its sensitivity to a nonlinear parameter and polishes the
 * converged point with one undamped step.
 *
 * One evaluation is a pass over the data that writes the basis functions
 * and their derivatives as columns (the model's vectorisable basis_columns
 * when it has one, otherwise basis_and_grad per point), a reduction to the
 * normal equations of the linear parameters, the small solve, and a second
 * reduction over the columns for the projected residual.
 *
 * Keep one instance per thread: the buffers are reused between pixels so
 * that after the first fit no memory is allocated.
 *
 * - Fixed nonlinear parameters are left out of the iteration. A fixed
 *   linear parameter keeps its value and moves to the data side of the
 *   linear solve.
 * - Limits on nonlinear parameters are handled by the driver. A limit on a
 *   linear parameter cannot be enforced by the linear solve: when the
 *   solution violates one, the fit reports failure so that the caller can
 *   fall back to LevenbergMarquardt.
 * - Start values of free linear parameters are ignored.
 * - max_calls counts evaluations of the projected problem, as in
 *   LevenbergMarquardt; 0 selects 200 + 100 * npar + 5 * npar^2.
 * - Errors are the Gauss-Newton estimates of the full model at the
 *   solution, sqrt(diag((J^T J)^-1)) over the free parameters that do not
 *   sit on a limit, the same as LevenbergMarquardt reports.
 */
template <typename Model> class VariableProjection {
  public:
    static_assert(model::is_separable<Model>::value,
                  "VariableProjection needs a model with linear_par and "
                  "basis_and_grad");
    using Traits = model::separable_traits<Model>;
    static constexpr int npar = static_cast<int>(Model::npar);
    static constexpr int nlin = static_cast<int>(Traits::nlin);
    static constexpr int nnl = static_cast<int>(Traits::nnl);
    static_assert(Model::npar >= 1 && Model::npar <= 16,
                  "VariableProjection keeps npar x npar matrices on the stack");
    static_assert(nlin >= 1, "a separable model has at least one linear "
                             "parameter");

    using Result = MinimizerResult;
    // The driver iterates over the nonlinear parameters. A fully linear
    // model has none and is solved without iterating.
    static constexpr int ndrv = nnl > 0 ? nnl : 1;
    static constexpr int ncol = nnl * nlin > 0 ? nnl *nlin : 1;
    using Driver = DampedGaussNewton<ndrv>;
    using Normal = typename Driver::Normal;

    /**
     * @brief Fit one pixel, see LevenbergMarquardt::fit for the arguments.
     */
    Result fit(const FitModel<Model> &model, NDView<double, 1> x,
               NDView<double, 1> y, NDView<double, 1> s,
               const std::array<double, Model::npar> &start, double *par_out,
               double *err_out) {
        x_ = x;
        y_ = y;
        const auto n = static_cast<std::size_t>(x.size());
        phi_.resize(n * nlin);
        dphi_.resize(n * nlin * nnl);
        weighted_ = s.size() > 0;
        if (weighted_) {
            // 1 / s_i^2 once per pixel; points with s_i == 0 are ignored.
            w2_.resize(n);
            for (std::size_t i = 0; i < n; ++i) {
                const double si = s[static_cast<ssize_t>(i)];
                w2_[i] = si != 0.0 ? 1.0 / (si * si) : 0.0;
            }
        }
        pvec_.assign(start.begin(), start.end());
        have_last_ = false;
        nfree_lin_ = 0;
        for (int j = 0; j < nlin; ++j) {
            const auto k = static_cast<unsigned int>(Traits::linear_index(j));
            lin_fixed_[j] = model.is_user_fixed(k);
            if (!lin_fixed_[j])
                free_lin_[nfree_lin_++] = j;
        }
        std::array<double, ndrv> alpha{};
        std::array<double, ndrv> lower{};
        std::array<double, ndrv> upper{};
        std::array<bool, ndrv> fixed{};
        for (int k = 0; k < nnl; ++k) {
            const auto idx =
                static_cast<unsigned int>(Traits::nonlinear_par[k]);
            alpha[k] = start[idx];
            lower[k] = model.lower_limit(idx);
            upper[k] = model.upper_limit(idx);
            fixed[k] = model.is_user_fixed(idx);
        }
        const int max_calls = model.max_calls() > 0
                                  ? static_cast<int>(model.max_calls())
                                  : 200 + 100 * npar + 5 * npar * npar;

        Result res;
        auto fail = [&]() {
            res.valid = false;
            res.chi2 = 0.0;
            std::fill(par_out, par_out + npar, 0.0);
            if (err_out)
                std::fill(err_out, err_out + npar, 0.0);
            return res;
        };

        if constexpr (nnl == 0) {
            Normal nrm;
            if (!evaluate(alpha.data(), nrm))
                return fail();
            res.calls = 1;
            res.valid = true;
            res.chi2 = 2.0 * nrm.F;
            for (int j = 0; j < nlin; ++j)
                pvec_[Traits::linear_index(j)] =
                    nrm.aux[static_cast<std::size_t>(j)];
        } else {
            auto eval = [this](const double *a, Normal &out) {
                return evaluate(a, out);
            };
            typename Driver::Options opt;
            opt.tolerance = model.tolerance();
            opt.max_calls = max_calls;
            // The same cautious start as LevenbergMarquardt: a large first
            // step can still drive a width to zero. Rejecting degenerate
            // trial points guards against that, and the polish step makes
            // up for the few iterations, which stop while the damping still
            // shortens the steps.
            opt.damping0 = 0.1;
            opt.reject_degenerate = true;
            opt.polish = true;
            res = driver_.fit(eval, alpha.data(), lower.data(), upper.data(),
                              fixed.data(), opt);
            if (!res.valid)
                return fail();
            for (int k = 0; k < nnl; ++k)
                pvec_[Traits::nonlinear_par[k]] = driver_.point()[k];
            for (int j = 0; j < nlin; ++j)
                pvec_[Traits::linear_index(j)] =
                    driver_.normal().aux[static_cast<std::size_t>(j)];
        }

        // The linear solve knows nothing about limits.
        for (int j = 0; j < nlin; ++j) {
            if (lin_fixed_[j])
                continue;
            const auto k = static_cast<unsigned int>(Traits::linear_index(j));
            if (pvec_[k] < model.lower_limit(k) ||
                pvec_[k] > model.upper_limit(k))
                return fail();
        }
        std::copy(pvec_.begin(), pvec_.end(), par_out);
        if (err_out)
            errors(model, err_out);
        return res;
    }

  private:
    // F = chi2 / 2, gradient and Kaufman Hessian of the projected residual
    // at the nonlinear point alpha, and the linear solution in out.aux.
    // Returns false when the model rejects the point or the basis functions
    // are linearly dependent there.
    bool evaluate(const double *alpha, Normal &out) {
        for (int k = 0; k < nnl; ++k)
            pvec_[Traits::nonlinear_par[k]] = alpha[k];
        if (!Model::is_valid(pvec_))
            return false;
        const ssize_t n = x_.size();

        // Basis functions and their derivatives, one column per function:
        // phi_[j * n + i] and dphi_[(k * nlin + j) * n + i].
        if constexpr (model::has_basis_columns<Model>::value) {
            Model::basis_columns(x_.data(), n, pvec_, phi_.data(),
                                 dphi_.data());
        } else {
            typename Traits::Basis phi{};
            typename Traits::BasisGrad dphi{};
            double *pc = phi_.data();
            double *dc = dphi_.data();
            const auto un = static_cast<std::size_t>(n);
            for (ssize_t i = 0; i < n; ++i) {
                Model::basis_and_grad(x_[i], pvec_, phi, dphi);
                const auto ui = static_cast<std::size_t>(i);
                for (int j = 0; j < nlin; ++j)
                    pc[static_cast<std::size_t>(j) * un + ui] =
                        phi[static_cast<std::size_t>(j)];
                for (int k = 0; k < nnl; ++k)
                    for (int j = 0; j < nlin; ++j)
                        dc[static_cast<std::size_t>(k * nlin + j) * un + ui] =
                            dphi[static_cast<std::size_t>(k)]
                                [static_cast<std::size_t>(j)];
            }
        }
        return weighted_ ? reduce<true>(alpha, out) : reduce<false>(alpha, out);
    }

    // The reductions over the columns: normal equations and solve of the
    // linear parameters, then the projected residual. All loops over the
    // linear and nonlinear parameters have compile-time bounds; the free
    // linear block is packed out of the full sums afterwards.
    template <bool Weighted> bool reduce(const double *alpha, Normal &out) {
        const ssize_t n = x_.size();
        const auto un = static_cast<std::size_t>(n);
        const double *pc[nlin];
        for (int j = 0; j < nlin; ++j)
            pc[j] = phi_.data() + static_cast<std::size_t>(j) * un;
        const double *dc[ncol];
        for (int c = 0; c < nnl * nlin; ++c)
            dc[c] = dphi_.data() + static_cast<std::size_t>(c) * un;
        const double *yd = y_.data();
        const double *w2 = Weighted ? w2_.data() : nullptr;

        // Pass 1: normal equations M = Phi^T W Phi, t = Phi^T W y of all
        // linear parameters.
        double M[nlin][nlin] = {};
        double t[nlin] = {};
        for (ssize_t i = 0; i < n; ++i) {
            const double wi = Weighted ? w2[i] : 1.0;
            const double yi = yd[i];
            double f[nlin];
            for (int j = 0; j < nlin; ++j)
                f[j] = pc[j][i];
            for (int a = 0; a < nlin; ++a) {
                const double fa = f[a] * wi;
                t[a] += fa * yi;
                for (int b = a; b < nlin; ++b)
                    M[a][b] += fa * f[b];
            }
        }

        // Free linear parameters: their block of M, with the fixed ones
        // moved to the right-hand side, v_a = t_a - sum_fixed c_j M_aj.
        double A[nlin][nlin];
        double v[nlin];
        for (int a = 0; a < nfree_lin_; ++a) {
            const int ja = free_lin_[a];
            double va = t[ja];
            for (int j = 0; j < nlin; ++j)
                if (lin_fixed_[j])
                    va -= pvec_[Traits::linear_index(j)] *
                          (ja <= j ? M[ja][j] : M[j][ja]);
            v[a] = va;
            for (int b = 0; b < nfree_lin_; ++b) {
                const int jb = free_lin_[b];
                A[a][b] = ja <= jb ? M[ja][jb] : M[jb][ja];
            }
        }
        double L[nlin][nlin];
        if (!cholesky_factor<nlin>(nfree_lin_, A, L))
            return false;
        double beta_free[nlin];
        cholesky_substitute<nlin>(nfree_lin_, L, v, beta_free);
        for (int a = 0; a < nfree_lin_; ++a)
            pvec_[Traits::linear_index(free_lin_[a])] = beta_free[a];
        double beta[nlin];
        for (int j = 0; j < nlin; ++j) {
            beta[j] = pvec_[Traits::linear_index(j)];
            out.aux[static_cast<std::size_t>(j)] = beta[j];
        }

        // Pass 2: residual r = y - Phi beta and u_k = df/dalpha_k at fixed
        // beta; F, b = U^T W r, N = U^T W U and Q = Phi^T W U.
        double F = 0.0;
        double b[ndrv] = {};
        double N[ndrv][ndrv] = {};
        double Q[ndrv][nlin] = {};
        for (ssize_t i = 0; i < n; ++i) {
            const double wi = Weighted ? w2[i] : 1.0;
            double f[nlin];
            for (int j = 0; j < nlin; ++j)
                f[j] = pc[j][i];
            double r = yd[i];
            for (int j = 0; j < nlin; ++j)
                r -= beta[j] * f[j];
            double u[ndrv] = {};
            for (int k = 0; k < nnl; ++k)
                for (int j = 0; j < nlin; ++j)
                    u[k] += beta[j] * dc[k * nlin + j][i];
            const double rw = r * wi;
            F += 0.5 * r * rw;
            for (int k = 0; k < nnl; ++k) {
                const double uw = u[k] * wi;
                b[k] += uw * r;
                for (int l = k; l < nnl; ++l)
                    N[k][l] += uw * u[l];
                for (int j = 0; j < nlin; ++j)
                    Q[k][j] += uw * f[j];
            }
        }

        // The full Gauss-Newton Hessian of the model at this point is
        // [[M, Q^T], [Q, N]]; kept for the errors of the final point.
        for (int k = 0; k < nnl; ++k)
            last_alpha_[k] = alpha[k];
        for (int a = 0; a < nlin; ++a)
            for (int c = a; c < nlin; ++c)
                last_M_[a][c] = M[a][c];
        for (int k = 0; k < nnl; ++k) {
            for (int j = 0; j < nlin; ++j)
                last_Q_[k][j] = Q[k][j];
            for (int l = k; l < nnl; ++l)
                last_N_[k][l] = N[k][l];
        }
        have_last_ = true;

        // Kaufman's Jacobian: U projected out of the span of the free basis,
        // H = N - Q_free^T A^-1 Q_free.
        double qf[ndrv][nlin];
        double z[ndrv][nlin];
        for (int k = 0; k < nnl; ++k) {
            for (int a = 0; a < nfree_lin_; ++a)
                qf[k][a] = Q[k][free_lin_[a]];
            cholesky_substitute<nlin>(nfree_lin_, L, qf[k], z[k]);
        }
        out.F = F;
        for (int k = 0; k < nnl; ++k) {
            out.g[k] = -b[k];
            for (int l = k; l < nnl; ++l) {
                double h = N[k][l];
                for (int a = 0; a < nfree_lin_; ++a)
                    h -= qf[k][a] * z[l][a];
                out.H[k][l] = h;
            }
        }
        return true;
    }

    // Gauss-Newton errors from the Jacobian of the full model at the
    // solution, the estimate LevenbergMarquardt reports as well. The
    // Jacobian's normal matrix is assembled from the sums of the last
    // evaluation when that was the final point, which it is unless the last
    // trial was rejected; otherwise one pass over the data computes it.
    void errors(const FitModel<Model> &model, double *err_out) {
        double H[npar][npar] = {};
        bool at_solution = have_last_;
        for (int k = 0; k < nnl; ++k)
            at_solution = at_solution &&
                          pvec_[Traits::nonlinear_par[k]] == last_alpha_[k];
        if (at_solution) {
            for (int a = 0; a < nlin; ++a)
                for (int c = a; c < nlin; ++c)
                    H[Traits::linear_index(a)][Traits::linear_index(c)] =
                        last_M_[a][c];
            for (int k = 0; k < nnl; ++k) {
                const auto m = static_cast<int>(Traits::nonlinear_par[k]);
                for (int j = 0; j < nlin; ++j) {
                    const auto i = static_cast<int>(Traits::linear_index(j));
                    H[std::min(i, m)][std::max(i, m)] = last_Q_[k][j];
                }
                for (int l = k; l < nnl; ++l)
                    H[m][Traits::nonlinear_par[l]] = last_N_[k][l];
            }
        } else {
            const ssize_t n = x_.size();
            std::array<double, Model::npar> g{};
            double f = 0.0;
            for (ssize_t i = 0; i < n; ++i) {
                Model::eval_and_grad(x_[i], pvec_, f, g);
                const double wi =
                    weighted_ ? w2_[static_cast<std::size_t>(i)] : 1.0;
                for (int a = 0; a < npar; ++a)
                    for (int c = a; c < npar; ++c)
                        H[a][c] += g[a] * g[c] * wi;
            }
        }
        bool skip[npar];
        for (int k = 0; k < npar; ++k)
            skip[k] = model.is_user_fixed(static_cast<unsigned int>(k));
        if constexpr (nnl > 0) {
            for (int k = 0; k < nnl; ++k)
                skip[Traits::nonlinear_par[k]] =
                    skip[Traits::nonlinear_par[k]] || driver_.skipped(k);
        }
        gauss_newton_errors<npar>(H, skip, err_out);
    }

    NDView<double, 1> x_, y_;
    Driver driver_;
    bool weighted_ = false;
    // Sums of the last evaluation, see errors().
    bool have_last_ = false;
    double last_alpha_[ndrv] = {};
    double last_M_[nlin][nlin] = {};
    double last_Q_[ndrv][nlin] = {};
    double last_N_[ndrv][ndrv] = {};
    int nfree_lin_ = 0;
    int free_lin_[nlin] = {};
    bool lin_fixed_[nlin] = {};
    std::vector<double> phi_;  // nlin columns of n basis values
    std::vector<double> dphi_; // nnl * nlin columns of their derivatives
    std::vector<double> w2_;   // 1 / s_i^2 for weighted fits
    std::vector<double> pvec_;
};

} // namespace aare::detail
