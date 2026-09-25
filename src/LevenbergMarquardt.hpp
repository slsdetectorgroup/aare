// SPDX-License-Identifier: MPL-2.0
#pragma once

#include "aare/FitModel.hpp"
#include "aare/NDView.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <limits>
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
 * - Fixed parameters are left out of the linear system.
 * - Limits: a trial step that leaves the allowed box is reflected at the
 *   limit. The step truncated at the limit is evaluated as well and the
 *   better of the two is kept. A parameter sitting on a limit whose gradient
 *   points outward is frozen for that iteration.
 * - Damping: Marquardt scaling u * diag(J^T J), starting at u = 0.1, with the
 *   update rule of Nielsen (Madsen, Nielsen and Tingleff, Methods for
 *   Non-Linear Least Squares Problems, 2004).
 * - Convergence follows Minuit's convention: the fit stops when the
 *   estimated distance to the minimum of chi2 (EDM, computed from the
 *   Gauss-Newton Hessian) drops below 0.002 * tolerance, or when the step
 *   becomes negligible.
 * - max_calls limits the number of model evaluations: the initial point,
 *   every trial point and every accepted point count. As in Minuit2, 0
 *   selects the default budget 200 + 100 * npar + 5 * npar^2.
 * - Errors are Gauss-Newton estimates, sqrt(diag((J^T J)^-1)), over the free
 *   parameters that do not sit on a limit. Fixed and limit-bound parameters
 *   report 0.
 */
template <typename Model> class LevenbergMarquardt {
  public:
    static constexpr int npar = static_cast<int>(Model::npar);
    static_assert(Model::npar >= 1 && Model::npar <= 16,
                  "LevenbergMarquardt keeps npar x npar matrices on the stack");

    struct Result {
        bool valid = false;
        int calls = 0;
        int iterations = 0;
        double chi2 = 0.0;
    };

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
        nfree_ = 0;
        for (int k = 0; k < npar; ++k) {
            const auto idx = static_cast<unsigned int>(k);
            lower_[k] = model.lower_limit(idx);
            upper_[k] = model.upper_limit(idx);
            p_[k] = start[static_cast<std::size_t>(k)];
            if (!model.is_user_fixed(idx))
                free_[nfree_++] = k;
        }
        for (int a = 0; a < nfree_; ++a)
            q_[a] = p_[free_[a]];
        r_.resize(n);
        r_new_.resize(n);
        J_.resize(n * static_cast<std::size_t>(npar));

        Result res;
        auto fail = [&]() {
            std::fill(par_out, par_out + npar, 0.0);
            if (err_out)
                std::fill(err_out, err_out + npar, 0.0);
            return res;
        };

        if (!evaluate(q_, r_, true, F_))
            return fail();
        res.calls = 1;
        const double edm_target = 0.002 * model.tolerance();
        const int max_calls = model.max_calls() > 0
                                  ? static_cast<int>(model.max_calls())
                                  : 200 + 100 * npar + 5 * npar * npar;
        double u = 0.1; // damping relative to diag(J^T J)
        double nu = 2.0;

        for (;; ++res.iterations) {
            build_normal_equations();
            mark_active();
            if (edm() < edm_target || gmax_ < 1e-12) {
                res.valid = true;
                break;
            }
            if (res.calls >= max_calls)
                break;
            if (!solve_step(u)) {
                u *= nu;
                nu *= 2.0;
                if (u > 1e32)
                    break;
                continue;
            }
            double step2 = 0.0;
            double q2 = 0.0;
            bool crossed = false;
            for (int a = 0; a < nfree_; ++a) {
                const int k = free_[a];
                const double v = q_[a] + delta_[a];
                const double truncated = std::clamp(v, lower_[k], upper_[k]);
                double reflected = v;
                if (truncated != v) {
                    crossed = true;
                    if (v < lower_[k])
                        reflected = 2.0 * lower_[k] - v;
                    if (v > upper_[k])
                        reflected = 2.0 * upper_[k] - v;
                    reflected = std::clamp(reflected, lower_[k], upper_[k]);
                }
                q_new_[a] = reflected;
                q_alt_[a] = truncated;
                const double d = q_new_[a] - q_[a];
                step2 += d * d;
                q2 += q_[a] * q_[a];
            }
            if (std::sqrt(step2) <= 1e-8 * (std::sqrt(q2) + 1e-8)) {
                res.valid = true;
                break;
            }
            double F_new = 0.0;
            ++res.calls;
            bool ok = evaluate(q_new_, r_new_, false, F_new);
            if (crossed) {
                // Also try the step truncated at the limit, keep the better.
                double F_alt = 0.0;
                ++res.calls;
                if (evaluate(q_alt_, r_new_, false, F_alt) &&
                    (!ok || F_alt < F_new)) {
                    q_new_ = q_alt_;
                    F_new = F_alt;
                    ok = true;
                }
            }
            const double rho = ok ? (F_ - F_new) / predicted_ : -1.0;
            if (rho > 0.0) {
                q_ = q_new_;
                ++res.calls;
                if (!evaluate(q_, r_, true, F_))
                    break; // same point as the accepted trial, cannot happen
                const double t = 2.0 * rho - 1.0;
                u *= std::max(1.0 / 3.0, 1.0 - t * t * t);
                nu = 2.0;
            } else {
                u *= nu;
                nu *= 2.0;
                if (u > 1e32)
                    break;
            }
        }
        if (!res.valid)
            return fail();

        // p_ tracks the last evaluated point, which may be a rejected trial.
        for (int a = 0; a < nfree_; ++a)
            p_[free_[a]] = q_[a];
        for (int k = 0; k < npar; ++k)
            par_out[k] = p_[k];
        res.chi2 = 2.0 * F_;

        if (err_out) {
            std::fill(err_out, err_out + npar, 0.0);
            double A[npar][npar];
            double b[npar];
            double inv[npar][npar];
            const int nact = pack(A, b, 0.0);
            if (nact > 0 && invert_spd(nact, A, inv)) {
                int i = 0;
                for (int a = 0; a < nfree_; ++a) {
                    if (active_[a])
                        continue;
                    err_out[free_[a]] = std::sqrt(std::max(inv[i][i], 0.0));
                    ++i;
                }
            }
        }
        return res;
    }

  private:
    double weight(ssize_t i) const {
        return weighted_ ? (s_[i] != 0.0 ? 1.0 / s_[i] : 0.0) : 1.0;
    }

    // Residuals r = (y - f) / s and, optionally, the Jacobian dr/dp for the
    // free parameters at the point q. F = chi2 / 2. Returns false when the
    // model rejects the parameters.
    bool evaluate(const std::array<double, Model::npar> &q,
                  std::vector<double> &r, bool with_jacobian, double &F) {
        for (int a = 0; a < nfree_; ++a)
            p_[free_[a]] = q[a];
        pvec_.assign(p_.begin(), p_.end());
        if (!Model::is_valid(pvec_))
            return false;
        const ssize_t n = x_.size();
        F = 0.0;
        if (with_jacobian) {
            std::array<double, Model::npar> g{};
            double f = 0.0;
            for (ssize_t i = 0; i < n; ++i) {
                const double w = weight(i);
                Model::eval_and_grad(x_[i], pvec_, f, g);
                r[i] = (y_[i] - f) * w;
                F += 0.5 * r[i] * r[i];
                double *Ji = &J_[static_cast<std::size_t>(i) * npar];
                for (int a = 0; a < nfree_; ++a)
                    Ji[a] = -g[free_[a]] * w;
            }
        } else {
            for (ssize_t i = 0; i < n; ++i) {
                r[i] = (y_[i] - Model::eval(x_[i], pvec_)) * weight(i);
                F += 0.5 * r[i] * r[i];
            }
        }
        return true;
    }

    void build_normal_equations() {
        const ssize_t n = x_.size();
        for (int a = 0; a < nfree_; ++a) {
            g_[a] = 0.0;
            for (int b = 0; b < nfree_; ++b)
                jtj_[a][b] = 0.0;
        }
        for (ssize_t i = 0; i < n; ++i) {
            const double *Ji = &J_[static_cast<std::size_t>(i) * npar];
            for (int a = 0; a < nfree_; ++a) {
                g_[a] += Ji[a] * r_[i];
                for (int b = a; b < nfree_; ++b)
                    jtj_[a][b] += Ji[a] * Ji[b];
            }
        }
        gmax_ = 0.0;
        for (int a = 0; a < nfree_; ++a) {
            for (int b = 0; b < a; ++b)
                jtj_[a][b] = jtj_[b][a];
            gmax_ = std::max(gmax_, std::abs(g_[a]));
        }
    }

    // A parameter sitting on a limit with the descent direction pointing
    // outward is frozen for this iteration.
    void mark_active() {
        for (int a = 0; a < nfree_; ++a) {
            const int k = free_[a];
            active_[a] = (p_[k] <= lower_[k] && g_[a] > 0.0) ||
                         (p_[k] >= upper_[k] && g_[a] < 0.0);
        }
    }

    // Minuit's estimated distance to the minimum of chi2, using the
    // Gauss-Newton Hessian: g^T (J^T J)^-1 g with g the gradient of chi2/2.
    double edm() {
        double A[npar][npar];
        double b[npar];
        double z[npar];
        const int m = pack(A, b, 0.0);
        if (m == 0)
            return 0.0;
        if (!cholesky_solve(m, A, b, z))
            return std::numeric_limits<double>::infinity();
        double e = 0.0;
        for (int i = 0; i < m; ++i)
            e += b[i] * z[i];
        return e;
    }

    // Copy the non-frozen block of J^T J (+ u * diag) and g into A and b.
    int pack(double A[npar][npar], double b[npar], double u) const {
        double mean_diag = 0.0;
        for (int a = 0; a < nfree_; ++a)
            mean_diag += jtj_[a][a];
        mean_diag = std::max(mean_diag / std::max(nfree_, 1), 1e-300);
        int m = 0;
        for (int a = 0; a < nfree_; ++a) {
            if (active_[a])
                continue;
            int mb = 0;
            for (int c = 0; c < nfree_; ++c) {
                if (active_[c])
                    continue;
                A[m][mb++] = jtj_[a][c];
            }
            A[m][m] += u * std::max(jtj_[a][a], 1e-10 * mean_diag);
            b[m] = g_[a];
            ++m;
        }
        return m;
    }

    // Damped Gauss-Newton step (J^T J + u D) delta = -g for the non-frozen
    // parameters; also the predicted decrease of F for that step.
    bool solve_step(double u) {
        double A[npar][npar];
        double b[npar];
        double z[npar];
        const int m = pack(A, b, u);
        for (int a = 0; a < nfree_; ++a)
            delta_[a] = 0.0;
        predicted_ = 0.0;
        if (m == 0)
            return true;
        if (!cholesky_solve(m, A, b, z))
            return false;
        int i = 0;
        for (int a = 0; a < nfree_; ++a) {
            if (active_[a])
                continue;
            delta_[a] = -z[i];
            predicted_ +=
                0.5 * delta_[a] *
                (u * std::max(jtj_[a][a], 1e-300) * delta_[a] - g_[a]);
            ++i;
        }
        return predicted_ > 0.0;
    }

    static bool cholesky_solve(int m, const double A[npar][npar],
                               const double *b, double *z) {
        double L[npar][npar];
        for (int i = 0; i < m; ++i) {
            for (int j = 0; j <= i; ++j) {
                double sum = A[i][j];
                for (int k = 0; k < j; ++k)
                    sum -= L[i][k] * L[j][k];
                if (i == j) {
                    if (!(sum > 0.0))
                        return false;
                    L[i][i] = std::sqrt(sum);
                } else {
                    L[i][j] = sum / L[j][j];
                }
            }
        }
        double t[npar];
        for (int i = 0; i < m; ++i) {
            double sum = b[i];
            for (int k = 0; k < i; ++k)
                sum -= L[i][k] * t[k];
            t[i] = sum / L[i][i];
        }
        for (int i = m - 1; i >= 0; --i) {
            double sum = t[i];
            for (int k = i + 1; k < m; ++k)
                sum -= L[k][i] * z[k];
            z[i] = sum / L[i][i];
        }
        return true;
    }

    static bool invert_spd(int m, const double A[npar][npar],
                           double inv[npar][npar]) {
        double e[npar];
        double col[npar];
        for (int j = 0; j < m; ++j) {
            for (int i = 0; i < m; ++i)
                e[i] = (i == j) ? 1.0 : 0.0;
            if (!cholesky_solve(m, A, e, col))
                return false;
            for (int i = 0; i < m; ++i)
                inv[i][j] = col[i];
        }
        return true;
    }

    NDView<double, 1> x_, y_, s_;
    bool weighted_ = false;
    int nfree_ = 0;
    int free_[npar] = {};
    bool active_[npar] = {};
    double lower_[npar] = {};
    double upper_[npar] = {};
    std::array<double, Model::npar> p_{};
    std::array<double, Model::npar> q_{};
    std::array<double, Model::npar> q_new_{};
    std::array<double, Model::npar> q_alt_{};
    double g_[npar] = {};
    double delta_[npar] = {};
    double jtj_[npar][npar] = {};
    double F_ = 0.0;
    double gmax_ = 0.0;
    double predicted_ = 0.0;
    std::vector<double> r_;
    std::vector<double> r_new_;
    std::vector<double> J_;
    std::vector<double> pvec_;
};

} // namespace aare::detail
