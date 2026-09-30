// SPDX-License-Identifier: MPL-2.0
#pragma once

#include "DampedGaussNewton.hpp"
#include "FitHelpers.hpp"
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
 * accumulates far fewer sums. The few iterations stop while the damping
 * still shortens the steps, so the driver polishes the converged point with
 * one undamped step.
 *
 * One evaluation is a pass over the data that writes the basis functions
 * and their derivatives as columns (the model's vectorisable
 * basis_columns), a reduction to the normal equations of the linear
 * parameters, the small solve, and a second reduction over the columns for
 * the projected residual.
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
 * - max_calls counts evaluations of the projected problem, see
 *   evaluation_budget().
 * - Errors are the Gauss-Newton estimates of the full model at the
 *   solution, sqrt(diag((J^T J)^-1)) over the free parameters that do not
 *   sit on a limit, the same as LevenbergMarquardt reports.
 */
template <typename Model> class VariableProjection {
  public:
    static_assert(model::is_separable<Model>::value,
                  "VariableProjection needs a model with linear_par and "
                  "basis_columns");
    using Traits = model::separable_traits<Model>;
    static constexpr int npar = static_cast<int>(Model::npar);
    static constexpr int nlin = static_cast<int>(Traits::nlin);
    static constexpr int nnl = static_cast<int>(Traits::nnl);
    static_assert(Model::npar >= 1 && Model::npar <= 16,
                  "VariableProjection keeps npar x npar matrices on the stack");
    static_assert(nlin >= 1, "a separable model has at least one linear "
                             "parameter");

    // The driver iterates over the nonlinear parameters. A fully linear
    // model has none; it gets a single fixed one, which the driver solves
    // with the one evaluation of its start point.
    static constexpr int ndrv = nnl > 0 ? nnl : 1;
    static constexpr int ncol = nnl * nlin > 0 ? nnl *nlin : 1;

    // Kept with every evaluated point: the linear solution beta and the
    // blocks of the Gauss-Newton Hessian [[M, Q^T], [Q, N]] of the full
    // model there, which errors() needs at the final point.
    struct Solution {
        Vec<nlin> beta{};
        SymMat<nlin> M;      // Phi^T W Phi
        Mat<ndrv, nlin> Q{}; // U^T W Phi
        SymMat<ndrv> N;      // U^T W U
    };
    using Driver = DampedGaussNewton<ndrv, Solution>;
    using Normal = typename Driver::Normal;

    /**
     * @brief Fit one pixel, see LevenbergMarquardt::fit for the arguments.
     */
    MinimizerResult fit(const FitModel<Model> &model, NDView<double, 1> x,
                        NDView<double, 1> y, NDView<double, 1> s,
                        const std::array<double, Model::npar> &start,
                        double *par_out, double *err_out) {
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
        have_ref_ = false;
        nfree_lin_ = 0;
        for (int j = 0; j < nlin; ++j) {
            const auto k = static_cast<unsigned int>(Traits::linear_index(j));
            lin_fixed_[j] = model.is_user_fixed(k);
            if (!lin_fixed_[j])
                free_lin_[nfree_lin_++] = j;
        }
        Vec<ndrv> alpha{};
        Vec<ndrv> lower{};
        Vec<ndrv> upper{};
        std::array<bool, ndrv> fixed;
        fixed.fill(true);
        for (int k = 0; k < nnl; ++k) {
            const auto idx =
                static_cast<unsigned int>(Traits::nonlinear_par[k]);
            alpha[k] = start[idx];
            lower[k] = model.lower_limit(idx);
            upper[k] = model.upper_limit(idx);
            fixed[k] = model.is_user_fixed(idx);
        }
        typename Driver::Options opt;
        opt.tolerance = model.tolerance();
        opt.max_calls = evaluation_budget(model);
        opt.polish = true;

        auto eval = [this](const Vec<ndrv> &a, Normal &out) {
            return evaluate(a, out);
        };
        MinimizerResult res =
            driver_.fit(eval, alpha, lower, upper, fixed, opt);
        if (!res.valid)
            return res;
        const Solution &solution = driver_.normal().aux;
        for (int k = 0; k < nnl; ++k)
            pvec_[Traits::nonlinear_par[k]] = driver_.point()[k];
        for (int j = 0; j < nlin; ++j)
            pvec_[Traits::linear_index(j)] = solution.beta[j];
        res.valid = within_linear_limits(model);
        if (!res.valid)
            return res;
        std::copy(pvec_.begin(), pvec_.end(), par_out);
        if (err_out)
            errors(model, solution, err_out);
        return res;
    }

  private:
    // The linear solve knows nothing about limits. A solution beyond a
    // limit fails; one within rounding of a limit is moved onto it, so that
    // it counts as on the limit whichever way the rounding went.
    bool within_linear_limits(const FitModel<Model> &model) {
        for (int j = 0; j < nlin; ++j) {
            if (lin_fixed_[j])
                continue;
            const auto k = static_cast<unsigned int>(Traits::linear_index(j));
            const double lo = model.lower_limit(k);
            const double hi = model.upper_limit(k);
            const double tol = 1e-12 * std::max(1.0, std::abs(pvec_[k]));
            if (pvec_[k] < lo - tol || pvec_[k] > hi + tol)
                return false;
            if (std::abs(pvec_[k] - lo) <= tol)
                pvec_[k] = lo;
            else if (std::abs(pvec_[k] - hi) <= tol)
                pvec_[k] = hi;
        }
        return true;
    }

    // F = chi2 / 2, gradient and Kaufman Hessian of the projected residual
    // at the nonlinear point alpha, and its Solution in out.aux.
    // Returns false when the model rejects the point, or when the basis
    // functions are numerically linearly dependent there or one of them has
    // vanished on the scan points, see reduce().
    bool evaluate(const Vec<ndrv> &alpha, Normal &out) {
        for (int k = 0; k < nnl; ++k)
            pvec_[Traits::nonlinear_par[k]] = alpha[k];
        if (!Model::is_valid(pvec_))
            return false;
        // Basis functions and their derivatives, one column per function:
        // phi_[j * n + i] and dphi_[(k * nlin + j) * n + i].
        Model::basis_columns(x_.data(), x_.size(), pvec_, phi_.data(),
                             dphi_.data());
        return weighted_ ? reduce<true>(out) : reduce<false>(out);
    }

    // The reductions over the columns: normal equations and solve of the
    // linear parameters, then the projected residual. All loops over the
    // linear and nonlinear parameters have compile-time bounds; the free
    // linear block is packed out of the full sums afterwards.
    //
    // The two passes over the data are written out, and the factorisation
    // is a member rather than a local, because the code gcc generates for
    // the second pass depends on the rest of this function: with a local
    // Cholesky, with SymMat::rank1_update in the first pass or with
    // compress() for the block A, the weighted S-curve fit executes 6 to
    // 10 % more instructions. Check BM_FitScurveWeightedVarPro with
    // perf stat when changing it.
    template <bool Weighted> bool reduce(Normal &out) {
        const ssize_t n = x_.size();
        const auto un = static_cast<std::size_t>(n);
        std::array<const double *, nlin> pc;
        for (int j = 0; j < nlin; ++j)
            pc[j] = phi_.data() + static_cast<std::size_t>(j) * un;
        std::array<const double *, ncol> dc;
        for (int c = 0; c < nnl * nlin; ++c)
            dc[c] = dphi_.data() + static_cast<std::size_t>(c) * un;
        const double *yd = y_.data();
        const double *w2 = Weighted ? w2_.data() : nullptr;

        // Pass 1: normal equations M = Phi^T W Phi, t = Phi^T W y of all
        // linear parameters.
        SymMat<nlin> M;
        Vec<nlin> t{};
        for (ssize_t i = 0; i < n; ++i) {
            const double wi = Weighted ? w2[i] : 1.0;
            const double yi = yd[i];
            Vec<nlin> f;
            for (int j = 0; j < nlin; ++j)
                f[j] = pc[j][i];
            for (int a = 0; a < nlin; ++a) {
                const double fa = f[a] * wi;
                t[a] += fa * yi;
                for (int b = a; b < nlin; ++b)
                    M.upper[a][b] += fa * f[b];
            }
        }

        // Free linear parameters: their block of M, with the fixed ones
        // moved to the right-hand side, v_a = t_a - sum_fixed c_j M_aj. The
        // loop runs over the compile-time bound and writes zeros past
        // nfree_lin_; gcc otherwise reports v as possibly uninitialised.
        SymMat<nlin> A;
        Vec<nlin> v{};
        for (int a = 0; a < nlin; ++a) {
            if (a >= nfree_lin_) {
                v[a] = 0.0;
                continue;
            }
            const int ja = free_lin_[a];
            double va = t[ja];
            for (int j = 0; j < nlin; ++j)
                if (lin_fixed_[j])
                    va -= pvec_[Traits::linear_index(j)] * M(ja, j);
            v[a] = va;
            for (int b = a; b < nfree_lin_; ++b)
                A.upper[a][b] = M(ja, free_lin_[b]);
        }
        // A free basis function that has all but vanished on the scan points
        // (a Gaussian narrower than the point spacing, centred between two
        // points) leaves its coefficient undetermined, and the solve returns
        // an absurd one that fits a single point. Its squared norm is compared
        // with that at the pixel's first evaluation, the start point.
        if (!have_ref_) {
            for (int j = 0; j < nlin; ++j)
                ref_norm2_[j] = M(j, j);
            have_ref_ = true;
        }
        for (int a = 0; a < nfree_lin_; ++a) {
            const int j = free_lin_[a];
            if (!(M(j, j) >= vanished_basis * ref_norm2_[j]))
                return false;
        }
        if (!cholesky_.factor(nfree_lin_, A))
            return false;
        // Nearly collinear basis functions: a pivot of the factorisation far
        // below its diagonal element means that the linear solution loses
        // about log10(diagonal / pivot) digits, beyond which it is noise.
        if (!cholesky_.pivots_at_least(collinear_basis))
            return false;
        const bool refine = !cholesky_.pivots_at_least(refine_basis);
        Vec<nlin> beta_free{};
        cholesky_.solve(v, beta_free);
        for (int a = 0; a < nfree_lin_; ++a)
            pvec_[Traits::linear_index(free_lin_[a])] = beta_free[a];
        // Short of that, the digits lost to the normal equations are
        // recovered by one step of iterative refinement on the data (the
        // corrected semi-normal equations): the correction solves the same
        // equations for Phi^T W r of the residual of the first solution. A
        // polynomial fitted far from x = 0 needs it; x in [20000, 20500]
        // otherwise costs a Pol2 six digits.
        if (refine) {
            Vec<nlin> beta;
            for (int j = 0; j < nlin; ++j)
                beta[j] = pvec_[Traits::linear_index(j)];
            Vec<nlin> s{};
            for (ssize_t i = 0; i < n; ++i) {
                double r = yd[i];
                for (int j = 0; j < nlin; ++j)
                    r -= beta[j] * pc[j][i];
                const double rw = Weighted ? r * w2[i] : r;
                for (int j = 0; j < nlin; ++j)
                    s[j] += pc[j][i] * rw;
            }
            Vec<nlin> sf{};
            compress<nlin>(nlin, s, lin_fixed_, sf);
            Vec<nlin> delta{};
            cholesky_.solve(sf, delta);
            for (int a = 0; a < nfree_lin_; ++a)
                pvec_[Traits::linear_index(free_lin_[a])] += delta[a];
        }
        Vec<nlin> beta;
        for (int j = 0; j < nlin; ++j)
            beta[j] = pvec_[Traits::linear_index(j)];

        // Pass 2: residual r = y - Phi beta and u_k = df/dalpha_k at fixed
        // beta; F, b = U^T W r, N = U^T W U and Q = Phi^T W U.
        double F = 0.0;
        Vec<ndrv> b{};
        SymMat<ndrv> N;
        Mat<ndrv, nlin> Q{};
        for (ssize_t i = 0; i < n; ++i) {
            const double wi = Weighted ? w2[i] : 1.0;
            Vec<nlin> f;
            for (int j = 0; j < nlin; ++j)
                f[j] = pc[j][i];
            double r = yd[i];
            for (int j = 0; j < nlin; ++j)
                r -= beta[j] * f[j];
            Vec<ndrv> u{};
            for (int k = 0; k < nnl; ++k)
                for (int j = 0; j < nlin; ++j)
                    u[k] += beta[j] * dc[k * nlin + j][i];
            const double rw = r * wi;
            F += 0.5 * r * rw;
            for (int k = 0; k < nnl; ++k) {
                const double uw = u[k] * wi;
                b[k] += uw * r;
                for (int l = k; l < nnl; ++l)
                    N.upper[k][l] += uw * u[l];
                for (int j = 0; j < nlin; ++j)
                    Q[k][j] += uw * f[j];
            }
        }

        out.aux.beta = beta;
        out.aux.M = M;
        out.aux.Q = Q;
        out.aux.N = N;

        // Kaufman's Jacobian: U projected out of the span of the free basis,
        // H = N - Q_free^T A^-1 Q_free.
        Mat<ndrv, nlin> qf{};
        Mat<ndrv, nlin> z{};
        for (int k = 0; k < nnl; ++k) {
            compress<nlin>(nlin, Q[k], lin_fixed_, qf[k]);
            cholesky_.solve(qf[k], z[k]);
        }
        out.F = F;
        for (int k = 0; k < nnl; ++k) {
            out.g[k] = -b[k];
            for (int l = k; l < nnl; ++l) {
                double h = N(k, l);
                for (int a = 0; a < nfree_lin_; ++a)
                    h -= qf[k][a] * z[l][a];
                out.H(k, l) = h;
            }
        }
        return true;
    }

    // Gauss-Newton errors from the Jacobian of the full model at the
    // solution, the estimate LevenbergMarquardt reports as well. The
    // Jacobian's normal matrix is assembled from the sums that the
    // evaluation of the final point left in its Solution.
    void errors(const FitModel<Model> &model, const Solution &solution,
                double *err_out) const {
        SymMat<npar> H;
        const auto lin = [](int j) {
            return static_cast<int>(Traits::linear_index(j));
        };
        const auto nonlin = [](int k) {
            return static_cast<int>(Traits::nonlinear_par[k]);
        };
        for (int a = 0; a < nlin; ++a)
            for (int c = a; c < nlin; ++c)
                H(lin(a), lin(c)) = solution.M(a, c);
        for (int k = 0; k < nnl; ++k) {
            for (int j = 0; j < nlin; ++j)
                H(lin(j), nonlin(k)) = solution.Q[k][j];
            for (int l = k; l < nnl; ++l)
                H(nonlin(k), nonlin(l)) = solution.N(k, l);
        }
        // Fixed parameters and parameters that end on a limit have no error.
        // The driver knows the nonlinear ones; the linear solve can land a
        // linear parameter exactly on a limit without violating it.
        std::array<bool, npar> skip;
        for (int k = 0; k < npar; ++k) {
            const auto idx = static_cast<unsigned int>(k);
            const double v = pvec_[static_cast<std::size_t>(k)];
            skip[k] = model.is_user_fixed(idx) || v <= model.lower_limit(idx) ||
                      v >= model.upper_limit(idx);
        }
        for (int k = 0; k < nnl; ++k)
            skip[Traits::nonlinear_par[k]] =
                skip[Traits::nonlinear_par[k]] || driver_.skipped(k);
        gauss_newton_errors<npar>(H, skip, err_out);
    }

    NDView<double, 1> x_, y_;
    Driver driver_;
    bool weighted_ = false;
    // Squared norms of the basis functions at the start point, see reduce().
    bool have_ref_ = false;
    Vec<nlin> ref_norm2_{};
    static constexpr double vanished_basis = 1e-8;
    static constexpr double collinear_basis = 1e-10;
    // Pivot ratio below which the linear solution is refined, about four
    // lost digits.
    static constexpr double refine_basis = 1e-4;
    Cholesky<nlin> cholesky_; // of the free linear block, see reduce()
    int nfree_lin_ = 0;
    std::array<int, nlin> free_lin_{};
    std::array<bool, nlin> lin_fixed_{};
    std::vector<double> phi_;  // nlin columns of n basis values
    std::vector<double> dphi_; // nnl * nlin columns of their derivatives
    std::vector<double> w2_;   // 1 / s_i^2 for weighted fits
    std::vector<double> pvec_;
};

} // namespace aare::detail
