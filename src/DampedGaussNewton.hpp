// SPDX-License-Identifier: MPL-2.0
#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <limits>

namespace aare::detail {

/** @brief Outcome of one of the built-in minimizers. */
struct MinimizerResult {
    bool valid = false;
    int calls = 0;
    int iterations = 0;
    double chi2 = 0.0;
};

/**
 * @brief Cholesky factorisation of the leading m x m block of the symmetric
 * positive-definite matrix A. Writes the lower triangle of L with
 * A = L L^T. Returns false when the block is not positive definite.
 */
template <int N>
bool cholesky_factor(int m, const double A[N][N], double L[N][N]) {
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
    return true;
}

/** @brief Solves L L^T z = b for the leading m x m block of the factor L. */
template <int N>
void cholesky_substitute(int m, const double L[N][N], const double *b,
                         double *z) {
    double t[N];
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
}

/** @brief Solves A z = b for the leading m x m block of the SPD matrix A. */
template <int N>
bool cholesky_solve(int m, const double A[N][N], const double *b, double *z) {
    double L[N][N];
    if (!cholesky_factor<N>(m, A, L))
        return false;
    cholesky_substitute<N>(m, L, b, z);
    return true;
}

/** @brief Inverse of the leading m x m block of the SPD matrix A. */
template <int N>
bool invert_spd(int m, const double A[N][N], double inv[N][N]) {
    double e[N];
    double col[N];
    for (int j = 0; j < m; ++j) {
        for (int i = 0; i < m; ++i)
            e[i] = (i == j) ? 1.0 : 0.0;
        if (!cholesky_solve<N>(m, A, e, col))
            return false;
        for (int i = 0; i < m; ++i)
            inv[i][j] = col[i];
    }
    return true;
}

/**
 * @brief Gauss-Newton parameter errors sqrt(diag(H^-1)) over the parameters
 * that are not skipped, where H is the upper triangle of the Gauss-Newton
 * Hessian of chi2 / 2 over all N parameters. Skipped parameters (fixed, or
 * frozen on a limit) report 0. Returns false, leaving all errors 0, when the
 * block cannot be inverted.
 */
template <int N>
bool gauss_newton_errors(const double H[N][N], const bool *skip, double *err) {
    std::fill(err, err + N, 0.0);
    double A[N][N];
    double inv[N][N];
    int m = 0;
    for (int a = 0; a < N; ++a) {
        if (skip[a])
            continue;
        int mb = 0;
        for (int c = 0; c < N; ++c) {
            if (skip[c])
                continue;
            A[m][mb++] = a <= c ? H[a][c] : H[c][a];
        }
        ++m;
    }
    if (m == 0 || !invert_spd<N>(m, A, inv))
        return false;
    int i = 0;
    for (int a = 0; a < N; ++a) {
        if (skip[a])
            continue;
        err[a] = std::sqrt(std::max(inv[i][i], 0.0));
        ++i;
    }
    return true;
}

/**
 * @brief Damped Gauss-Newton iteration shared by the built-in minimizers.
 *
 * The problem is described by an evaluator called as
 * `eval(const double *p, Normal &out)`: at the point p (all NP parameters,
 * fixed ones at their values) it fills out.F = chi2 / 2, the gradient
 * out.g = dF/dp and the upper triangle of the Gauss-Newton Hessian out.H,
 * both over all NP parameters, and returns false when the point is not
 * admissible. LevenbergMarquardt evaluates residuals and the Jacobian of
 * the full model, VariableProjection the projected problem of the nonlinear
 * parameters. The driver extracts the free parameters and implements
 *
 * - Marquardt damping u * diag(H), starting at u = damping0, with the
 *   update rule of Nielsen (Madsen, Nielsen and Tingleff, Methods for
 *   Non-Linear Least Squares Problems, 2004). The damping also bounds the
 *   accuracy of the last accepted step, so a solver that converges in few
 *   iterations should start with a small damping.
 * - Limits: a trial step that leaves the allowed box is reflected at the
 *   limit. The step truncated at the limit is evaluated as well and the
 *   better of the two is kept. A parameter sitting on a limit whose gradient
 *   points outward is frozen for that iteration.
 * - Convergence in Minuit's convention: the iteration stops when the
 *   estimated distance to the minimum of chi2 (EDM, from the Gauss-Newton
 *   Hessian) drops below 0.002 * tolerance, or when the step becomes
 *   negligible.
 * - max_calls bounds the number of evaluations: the initial point and every
 *   trial point count, so an iteration costs one evaluation, or two when a
 *   limit is crossed.
 * - With reject_degenerate, a trial point at which the model has lost its
 *   sensitivity to a free parameter (its Hessian diagonal collapsed by more
 *   than eight orders of magnitude) is rejected like an inadmissible point,
 *   so the damping grows and a shorter step is tried instead.
 * - With polish, a converged fit takes one more undamped Gauss-Newton step
 *   and keeps it when it lowers chi2, see polish_step().
 *
 * After fit() the final point, its Normal and the frozen parameters remain
 * available for the error computation.
 */
template <int NP> class DampedGaussNewton {
  public:
    static_assert(NP >= 1 && NP <= 16,
                  "DampedGaussNewton keeps NP x NP matrices on the stack");

    struct Normal {
        double F = 0.0;               // chi2 / 2
        double g[NP] = {};            // dF/dp
        double H[NP][NP] = {};        // Gauss-Newton Hessian, upper triangle
        std::array<double, 16> aux{}; // evaluator payload, kept with the point
    };

    struct Options {
        double tolerance = 0.5; // EDM tolerance, Minuit's convention
        int max_calls = 100;    // budget of evaluations
        double damping0 = 0.1;  // initial Marquardt damping
        bool reject_degenerate = false;
        bool polish = false;
    };

    template <typename Eval>
    MinimizerResult fit(Eval &eval, const double *start, const double *lower,
                        const double *upper, const bool *fixed,
                        const Options &opt) {
        nfree_ = 0;
        for (int k = 0; k < NP; ++k) {
            lower_[k] = lower[k];
            upper_[k] = upper[k];
            p_[k] = start[k];
            fixed_[k] = fixed[k];
            free_pos_[k] = -1;
            if (!fixed[k]) {
                free_pos_[k] = nfree_;
                free_[nfree_++] = k;
            }
        }
        for (int a = 0; a < nfree_; ++a) {
            q_[a] = p_[free_[a]];
            active_[a] = false;
        }

        MinimizerResult res;
        if (!evaluate(eval, q_, cur_))
            return res;
        res.calls = 1;
        const double edm_target = 0.002 * opt.tolerance;
        const int max_calls = opt.max_calls;
        const bool reject_degenerate = opt.reject_degenerate;
        double u = opt.damping0; // damping relative to diag(H)
        double nu = 2.0;

        for (;; ++res.iterations) {
            extract_free();
            mark_active();
            if (gmax_ < 1e-12) {
                res.valid = true;
                break;
            }
            // Solve for the damped step first. The solve yields
            // g^T (H + u D)^-1 g, a lower bound on the EDM, so the undamped
            // factorisation behind edm() is only needed when that bound
            // leaves room for convergence.
            const bool have_step = solve_step(u);
            if (!have_step || bound_ < edm_margin * edm_target) {
                const double e = edm();
                if (e < edm_target) {
                    // A point already deep inside the tolerance needs no
                    // polishing.
                    if (opt.polish && e >= 0.01 * edm_target &&
                        res.calls < max_calls)
                        polish_step(eval, res, reject_degenerate);
                    res.valid = true;
                    break;
                }
            }
            if (res.calls >= max_calls)
                break;
            if (!have_step) {
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
            ++res.calls;
            bool ok = evaluate(eval, q_new_, trial_) &&
                      !(reject_degenerate && degenerate(trial_));
            if (crossed) {
                // Also try the step truncated at the limit, keep the better.
                ++res.calls;
                const bool ok_alt = evaluate(eval, q_alt_, alt_) &&
                                    !(reject_degenerate && degenerate(alt_));
                if (ok_alt && (!ok || alt_.F < trial_.F)) {
                    q_new_ = q_alt_;
                    std::swap(trial_, alt_);
                    ok = true;
                }
            }
            const double rho = ok ? (cur_.F - trial_.F) / predicted_ : -1.0;
            if (rho > 0.0) {
                // The trial becomes the current point.
                q_ = q_new_;
                std::swap(cur_, trial_);
                for (int a = 0; a < nfree_; ++a)
                    p_[free_[a]] = q_[a];
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
            return res;

        // p_ tracks the last evaluated point, which may be a rejected trial.
        for (int a = 0; a < nfree_; ++a)
            p_[free_[a]] = q_[a];
        res.chi2 = 2.0 * cur_.F;
        return res;
    }

    /** @brief The final point, all NP parameters. */
    const double *point() const { return p_; }

    /** @brief F, gradient and Hessian at the final point. */
    const Normal &normal() const { return cur_; }

    /** @brief True for a fixed parameter or one frozen on a limit. */
    bool skipped(int k) const {
        return fixed_[k] || (free_pos_[k] >= 0 && active_[free_pos_[k]]);
    }

    /** @brief Gauss-Newton errors at the final point, see
     * gauss_newton_errors(). */
    void errors(double *err_out) const {
        bool skip[NP];
        for (int k = 0; k < NP; ++k)
            skip[k] = skipped(k);
        gauss_newton_errors<NP>(cur_.H, skip, err_out);
    }

  private:
    template <typename Eval>
    bool evaluate(Eval &eval, const std::array<double, NP> &q, Normal &out) {
        for (int a = 0; a < nfree_; ++a)
            p_[free_[a]] = q[a];
        return eval(static_cast<const double *>(p_), out);
    }

    // Gradient and Hessian of the free parameters at the current point.
    void extract_free() {
        gmax_ = 0.0;
        for (int a = 0; a < nfree_; ++a) {
            const int ka = free_[a];
            g_[a] = cur_.g[ka];
            for (int b = 0; b < nfree_; ++b) {
                const int kb = free_[b];
                jtj_[a][b] = ka <= kb ? cur_.H[ka][kb] : cur_.H[kb][ka];
            }
            gmax_ = std::max(gmax_, std::abs(g_[a]));
        }
    }

    // A parameter sitting on a limit with the descent direction pointing
    // outward is frozen for this iteration. The test deliberately looks at
    // the last evaluated point p_, which after a rejected step is the trial
    // rather than the current point q_: a rejected trial that came off the
    // limit unfreezes the parameter, and the reflected step can then leave
    // the limit. Testing q_ instead was measured to leave more fits stuck on
    // a limit with a higher chi2 (FallingScurve and GaussianChargeSharing
    // cubes), although it helped Pol2.
    void mark_active() {
        for (int a = 0; a < nfree_; ++a) {
            const int k = free_[a];
            active_[a] = (p_[k] <= lower_[k] && g_[a] > 0.0) ||
                         (p_[k] >= upper_[k] && g_[a] < 0.0);
        }
    }

    // A trial point where the Hessian diagonal of a free parameter collapsed
    // relative to the current point: the model no longer responds to that
    // parameter there, so the iteration could stop at a spurious stationary
    // point (a step function with the width driven to zero, for example).
    bool degenerate(const Normal &trial) const {
        for (int a = 0; a < nfree_; ++a) {
            if (active_[a])
                continue;
            const int k = free_[a];
            if (!(trial.H[k][k] >= 1e-8 * cur_.H[k][k]))
                return true;
        }
        return false;
    }

    // Minuit's estimated distance to the minimum of chi2, using the
    // Gauss-Newton Hessian: g^T H^-1 g with g the gradient of chi2/2.
    // Also keeps the undamped Gauss-Newton step for polish_step().
    double edm() {
        double A[NP][NP];
        double b[NP];
        double z[NP];
        const int m = pack(A, b, 0.0);
        for (int a = 0; a < nfree_; ++a)
            gn_step_[a] = 0.0;
        if (m == 0)
            return 0.0;
        if (!cholesky_solve<NP>(m, A, b, z))
            return std::numeric_limits<double>::infinity();
        double e = 0.0;
        int i = 0;
        for (int a = 0; a < nfree_; ++a) {
            if (active_[a])
                continue;
            gn_step_[a] = -z[i];
            e += b[i] * z[i];
            ++i;
        }
        return e;
    }

    // One undamped Gauss-Newton step from a point that passed the EDM test,
    // kept when it lowers F. The test stops the iteration within the
    // tolerance, but a solver that converges in few iterations stops while
    // the damping is still noticeable, so its last accepted step fell short
    // of the minimum by about the damping factor. The extra evaluation lands
    // at the minimum to the accuracy of a full Gauss-Newton step.
    template <typename Eval>
    void polish_step(Eval &eval, MinimizerResult &res, bool reject_degenerate) {
        for (int a = 0; a < nfree_; ++a) {
            const int k = free_[a];
            q_new_[a] = std::clamp(q_[a] + gn_step_[a], lower_[k], upper_[k]);
        }
        ++res.calls;
        const bool ok = evaluate(eval, q_new_, trial_) &&
                        !(reject_degenerate && degenerate(trial_)) &&
                        trial_.F < cur_.F;
        if (ok) {
            q_ = q_new_;
            std::swap(cur_, trial_);
            extract_free();
        }
        for (int a = 0; a < nfree_; ++a)
            p_[free_[a]] = q_[a];
        if (ok)
            mark_active();
    }

    // Copy the non-frozen block of H (+ u * diag) and g into A and b.
    int pack(double A[NP][NP], double b[NP], double u) const {
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

    // Damped Gauss-Newton step (H + u D) delta = -g for the non-frozen
    // parameters, the predicted decrease of F for that step, and
    // bound_ = g^T (H + u D)^-1 g, which is at most the EDM because u D is
    // positive semi-definite.
    bool solve_step(double u) {
        double A[NP][NP];
        double b[NP];
        double z[NP];
        const int m = pack(A, b, u);
        for (int a = 0; a < nfree_; ++a)
            delta_[a] = 0.0;
        predicted_ = 0.0;
        bound_ = 0.0;
        if (m == 0)
            return true;
        if (!cholesky_solve<NP>(m, A, b, z))
            return false;
        int i = 0;
        for (int a = 0; a < nfree_; ++a) {
            if (active_[a])
                continue;
            delta_[a] = -z[i];
            bound_ += b[i] * z[i];
            predicted_ +=
                0.5 * delta_[a] *
                (u * std::max(jtj_[a][a], 1e-300) * delta_[a] - g_[a]);
            ++i;
        }
        return predicted_ > 0.0;
    }

    int nfree_ = 0;
    int free_[NP] = {};
    int free_pos_[NP] = {};
    bool fixed_[NP] = {};
    bool active_[NP] = {};
    double lower_[NP] = {};
    double upper_[NP] = {};
    double p_[NP] = {};
    std::array<double, NP> q_{};
    std::array<double, NP> q_new_{};
    std::array<double, NP> q_alt_{};
    double g_[NP] = {};
    double delta_[NP] = {};
    double gn_step_[NP] = {};
    double jtj_[NP][NP] = {};
    // The two solves that bound the EDM differ by rounding only when the
    // damping is negligible; the margin keeps the stop decision exact.
    static constexpr double edm_margin = 1.0 + 1e-6;
    double gmax_ = 0.0;
    double predicted_ = 0.0;
    double bound_ = 0.0;
    Normal cur_;
    Normal trial_;
    Normal alt_;
};

} // namespace aare::detail
