// SPDX-License-Identifier: MPL-2.0
#pragma once

#include "LinearAlgebra.hpp"

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
 * @brief Gauss-Newton parameter errors sqrt(diag(H^-1)) over the parameters
 * that are not skipped, where H is the Gauss-Newton Hessian of chi2 / 2 over
 * all N parameters. Skipped parameters (fixed, or frozen on a limit) report
 * 0. Returns false, leaving all errors 0, when the block cannot be inverted.
 */
template <int N>
bool gauss_newton_errors(const SymMat<N> &H, const std::array<bool, N> &skip,
                         double *err) {
    std::fill(err, err + N, 0.0);
    SymMat<N> A;
    Cholesky<N> cholesky;
    const int m = compress<N>(N, H, skip, A);
    if (m == 0 || !cholesky.factor(m, A))
        return false;
    Vec<N> variance{};
    cholesky.inverse_diag(variance);
    int i = 0;
    for (int a = 0; a < N; ++a) {
        if (skip[a])
            continue;
        err[a] = std::sqrt(std::max(variance[i], 0.0));
        ++i;
    }
    return true;
}

/** @brief Payload of an evaluator that keeps nothing with a point. */
struct NoPayload {};

/**
 * @brief Damped Gauss-Newton iteration shared by the built-in minimizers.
 *
 * The problem is described by an evaluator called as
 * `eval(const Vec<NP> &p, Normal &out)`: at the point p (all NP parameters,
 * fixed ones at their values) it fills out.F = chi2 / 2, the gradient
 * out.g = dF/dp and the Gauss-Newton Hessian out.H, both over all NP
 * parameters, and returns false when the point is not
 * admissible. It may store a Payload of its own in out.aux, which stays with
 * the point. LevenbergMarquardt evaluates residuals and the Jacobian of the
 * full model, VariableProjection the projected problem of the nonlinear
 * parameters. The driver extracts the free parameters and implements
 *
 * - Marquardt damping u * diag(H) with the update rule of Nielsen (Madsen,
 *   Nielsen and Tingleff, Methods for Non-Linear Least Squares Problems,
 *   2004). The start is cautious, u = 0.1: estimated start values can be far
 *   off, and the first steps are then wild.
 * - Limits: a trial step that leaves the allowed box is reflected at the
 *   limit. The step truncated at the limit is evaluated as well and the
 *   better of the two is kept. A parameter sitting on a limit whose gradient
 *   points outward is frozen for that iteration.
 * - Convergence in Minuit's convention: the iteration stops when the
 *   estimated distance to the minimum of chi2 (EDM, from the Gauss-Newton
 *   Hessian) drops below 0.002 * tolerance. A start point that passes
 *   already still takes one undamped step, see converged(). When the damped
 *   step becomes negligible for every parameter before that, the iteration
 *   has stalled and the fit is valid only if the EDM test passes at that
 *   point.
 * - max_calls bounds the number of evaluations: the initial point and every
 *   trial point count, so an iteration costs one evaluation, or two when a
 *   limit is crossed.
 * - A trial point at which the model has lost its sensitivity to a free
 *   parameter is rejected like an inadmissible point, so the damping grows
 *   and a shorter step is tried instead, see degenerate().
 * - With polish, a converged fit takes one more undamped Gauss-Newton step
 *   and keeps it when it lowers chi2, see polish_step(). The damping bounds
 *   the accuracy of the last accepted step, so a solver that converges in
 *   few iterations needs it.
 *
 * After fit() the final point, its Normal and the frozen parameters remain
 * available for the error computation.
 */
template <int NP, typename Payload = NoPayload> class DampedGaussNewton {
  public:
    static_assert(NP >= 1 && NP <= 16,
                  "DampedGaussNewton keeps NP x NP matrices on the stack");

    struct Normal {
        double F = 0.0; // chi2 / 2
        Vec<NP> g{};    // dF/dp
        SymMat<NP> H;   // Gauss-Newton Hessian
        Payload aux{};  // evaluator payload, kept with the point
    };

    struct Options {
        double tolerance = 0.5; // EDM tolerance, Minuit's convention
        int max_calls = 100;    // budget of evaluations
        bool polish = false;
    };

    template <typename Eval>
    MinimizerResult fit(Eval &eval, const Vec<NP> &start, const Vec<NP> &lower,
                        const Vec<NP> &upper, const std::array<bool, NP> &fixed,
                        const Options &opt) {
        opt_ = opt;
        lower_ = lower;
        upper_ = upper;
        fixed_ = fixed;
        p_ = start;
        nfree_ = 0;
        accepted_ = 0;
        for (int k = 0; k < NP; ++k) {
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
        for (int a = 0; a < nfree_; ++a)
            start_diag_[a] = cur_.H(free_[a], free_[a]);

        // With every parameter fixed the start point is the result.
        res.valid = nfree_ == 0 || iterate(eval, res);
        if (res.valid) {
            // p_ holds the last evaluated point, which may be a rejected
            // trial.
            for (int a = 0; a < nfree_; ++a)
                p_[free_[a]] = q_[a];
            res.chi2 = 2.0 * cur_.F;
        }
        return res;
    }

    /** @brief The final point, all NP parameters. */
    const Vec<NP> &point() const { return p_; }

    /** @brief F, gradient, Hessian and payload at the final point. */
    const Normal &normal() const { return cur_; }

    /**
     * @brief True for a parameter without an error: fixed, frozen on a limit,
     * or ending exactly on one. The frozen flag alone misses the last case,
     * because it needs the gradient to point outward and that gradient is
     * zero when the unconstrained minimum coincides with the limit.
     */
    bool skipped(int k) const {
        return fixed_[k] || (free_pos_[k] >= 0 && active_[free_pos_[k]]) ||
               p_[k] <= lower_[k] || p_[k] >= upper_[k];
    }

    /** @brief Gauss-Newton errors at the final point, see
     * gauss_newton_errors(). */
    void errors(double *err_out) const {
        std::array<bool, NP> skip;
        for (int k = 0; k < NP; ++k)
            skip[k] = skipped(k);
        gauss_newton_errors<NP>(cur_.H, skip, err_out);
    }

  private:
    // The iteration from the evaluated start point. Returns whether it
    // converged.
    template <typename Eval> bool iterate(Eval &eval, MinimizerResult &res) {
        const double edm_target = 0.002 * opt_.tolerance;
        double u = initial_damping; // damping relative to diag(H)
        double nu = 2.0;
        for (;; ++res.iterations) {
            extract_free();
            mark_active();
            if (gmax_ < 1e-12) {
                edm(); // the undamped step for converged()
                return converged(eval, res, false);
            }
            // Solve for the damped step first. The solve yields
            // g^T (H + u D)^-1 g, a lower bound on the EDM, so the undamped
            // factorisation behind edm() is only needed when that bound
            // leaves room for convergence.
            const bool have_step = solve_step(u);
            if (!have_step || bound_ < edm_margin * edm_target) {
                const double e = edm();
                // A point already deep inside the tolerance needs no
                // polishing.
                if (e < edm_target)
                    return converged(eval, res,
                                     opt_.polish && e >= 0.01 * edm_target);
            }
            if (res.calls >= opt_.max_calls)
                return false;
            double rho = -1.0; // gain ratio; without a step, grow the damping
            if (have_step) {
                // A stalled iteration is not a minimum: repeated rejections
                // shrink the damped step until it is negligible anywhere.
                // The test looks at the step before it is reflected or
                // truncated at a limit, so a step that a limit folds back
                // onto the current point still gets its truncated
                // alternative evaluated.
                if (negligible_step())
                    return edm() < edm_target;
                rho = try_step(eval, res);
            }
            if (rho > 0.0) {
                accept_trial();
                const double t = 2.0 * rho - 1.0;
                u *= std::max(1.0 / 3.0, 1.0 - t * t * t);
                nu = 2.0;
            } else {
                u *= nu;
                nu *= 2.0;
                if (u > 1e32)
                    return false;
            }
        }
    }

    template <typename Eval>
    bool evaluate(Eval &eval, const Vec<NP> &q, Normal &out) {
        for (int a = 0; a < nfree_; ++a)
            p_[free_[a]] = q[a];
        return eval(p_, out);
    }

    // Spends one evaluation on the trial point q. False when the point is
    // inadmissible or degenerate.
    template <typename Eval>
    bool try_point(Eval &eval, MinimizerResult &res, const Vec<NP> &q,
                   Normal &out) {
        ++res.calls;
        return evaluate(eval, q, out) && !degenerate(out);
    }

    // Evaluates the damped step from the current point, reflected at the
    // limits it crosses, and the step truncated at those limits as well;
    // the better of the two ends up in q_new_ and trial_. Returns its gain
    // ratio, the decrease of F over the predicted one, or a negative value
    // when neither point is admissible.
    template <typename Eval> double try_step(Eval &eval, MinimizerResult &res) {
        bool moved = false;
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
            moved = moved || reflected != q_[a];
        }
        // A reflection that lands on the current point needs no evaluation;
        // the truncated step is then the only candidate.
        bool ok = moved && try_point(eval, res, q_new_, trial_);
        if (crossed && res.calls < opt_.max_calls &&
            try_point(eval, res, q_alt_, alt_) && (!ok || alt_.F < trial_.F)) {
            q_new_ = q_alt_;
            std::swap(trial_, alt_);
            ok = true;
        }
        return ok ? (cur_.F - trial_.F) / predicted_ : -1.0;
    }

    // The trial becomes the current point.
    void accept_trial() {
        q_ = q_new_;
        std::swap(cur_, trial_);
        ++accepted_;
    }

    // Gradient and Hessian of the free parameters at the current point.
    void extract_free() {
        compress<NP>(NP, cur_.H, fixed_, jtj_);
        compress<NP>(NP, cur_.g, fixed_, g_);
        gmax_ = 0.0;
        for (int a = 0; a < nfree_; ++a)
            gmax_ = std::max(gmax_, std::abs(g_[a]));
    }

    // A parameter sitting on a limit with the descent direction pointing
    // outward is frozen for this iteration. Position and gradient are both
    // taken at the current point q_, so a parameter stays frozen at the end
    // only where the constrained minimum lies on its limit. An earlier
    // version tested the last evaluated point p_, a rejected trial, so that a
    // trial that came off a limit unfroze the parameter: testing q_ had left
    // more FallingScurve and GaussianChargeSharing fits stuck with the width
    // on its lower limit and a higher chi2. But pairing a trial position with
    // the current gradient could freeze an interior parameter and stop the
    // fit there. The width no longer reaches its limit that way, because
    // degenerate trial points are rejected, see degenerate().
    void mark_active() {
        for (int a = 0; a < nfree_; ++a) {
            const int k = free_[a];
            active_[a] = (q_[a] <= lower_[k] && g_[a] > 0.0) ||
                         (q_[a] >= upper_[k] && g_[a] < 0.0);
        }
    }

    // True when the damped step is negligible for every parameter that is
    // not frozen: below 1e-8 of its value plus its own scale 1/sqrt(H_aa),
    // the parameter change that raises chi2 by about one. Judging each
    // parameter on its own scale keeps a large parameter (an amplitude of
    // 1e10, a peak position at 1e9) from hiding the steps of the others.
    bool negligible_step() const {
        for (int a = 0; a < nfree_; ++a) {
            if (active_[a])
                continue;
            const double scale =
                std::abs(q_[a]) + 1.0 / std::sqrt(std::max(jtj_(a, a), 1e-300));
            if (!(std::abs(delta_[a]) <= 1e-8 * scale))
                return false;
        }
        return true;
    }

    // A trial point where the Hessian diagonal of a free parameter collapsed
    // by more than eight orders of magnitude relative to the current point
    // or to the start point: the model no longer responds to that parameter
    // there, so the iteration could stop at a spurious stationary point. A
    // width driven onto its lower limit, for example, turns a model into a
    // step function whose chi2 no longer responds to the width and jumps
    // with the position. The start point catches a collapse spread over
    // many accepted steps, each of which passes the test against its
    // predecessor.
    bool degenerate(const Normal &trial) const {
        for (int a = 0; a < nfree_; ++a) {
            if (active_[a])
                continue;
            const int k = free_[a];
            const double ref = std::max(cur_.H(k, k), start_diag_[a]);
            if (!(trial.H(k, k) >= 1e-8 * ref))
                return true;
        }
        return false;
    }

    // The convergence test passed. A point that passes it before any step
    // was accepted may merely lie within the tolerance of chi2's scale, as
    // the start estimate of an unweighted fit of data of small magnitude
    // does, so it still takes the undamped step of polish_step(), as Migrad
    // always iterates at least once. Without the budget for that step the
    // fit has not converged. With polish, a converged fit takes the same
    // step when the budget allows. Returns whether the fit is valid.
    template <typename Eval>
    bool converged(Eval &eval, MinimizerResult &res, bool polish) {
        const bool first = accepted_ == 0 && nfree_ > 0;
        if (!first && !polish)
            return true;
        if (res.calls >= opt_.max_calls)
            return !first;
        polish_step(eval, res);
        return true;
    }

    // One undamped Gauss-Newton step from a point that passed the EDM test,
    // kept when it lowers F. The test stops the iteration within the
    // tolerance, but a solver that converges in few iterations stops while
    // the damping is still noticeable, so its last accepted step fell short
    // of the minimum by about the damping factor. The extra evaluation lands
    // at the minimum to the accuracy of a full Gauss-Newton step.
    template <typename Eval>
    void polish_step(Eval &eval, MinimizerResult &res) {
        for (int a = 0; a < nfree_; ++a) {
            const int k = free_[a];
            q_new_[a] = std::clamp(q_[a] + gn_step_[a], lower_[k], upper_[k]);
        }
        if (try_point(eval, res, q_new_, trial_) && trial_.F < cur_.F) {
            accept_trial();
            extract_free();
            mark_active();
        }
    }

    // Copy the non-frozen block of H (+ u * diag) and g into A and b.
    int pack(SymMat<NP> &A, Vec<NP> &b, double u) const {
        const int m = compress<NP>(nfree_, jtj_, active_, A);
        compress<NP>(nfree_, g_, active_, b);
        double mean_diag = 0.0;
        for (int a = 0; a < nfree_; ++a)
            mean_diag += jtj_(a, a);
        mean_diag = std::max(mean_diag / std::max(nfree_, 1), 1e-300);
        int i = 0;
        for (int a = 0; a < nfree_; ++a) {
            if (active_[a])
                continue;
            A(i, i) += u * std::max(jtj_(a, a), 1e-10 * mean_diag);
            ++i;
        }
        return m;
    }

    // Solves (H + u D) z = g for the non-frozen parameters. Writes
    // step = -z, 0 for a frozen parameter, and gz = g^T z. Returns the
    // number of parameters solved for, or -1 when the matrix is not
    // positive definite.
    int solve(double u, Vec<NP> &step, double &gz) const {
        SymMat<NP> A;
        Vec<NP> b{};
        Vec<NP> z{};
        Cholesky<NP> cholesky;
        const int m = pack(A, b, u);
        for (int a = 0; a < nfree_; ++a)
            step[a] = 0.0;
        gz = 0.0;
        if (m > 0 && !cholesky.factor(m, A))
            return -1;
        cholesky.solve(b, z);
        int i = 0;
        for (int a = 0; a < nfree_; ++a) {
            if (active_[a])
                continue;
            step[a] = -z[i];
            gz += b[i] * z[i];
            ++i;
        }
        return m;
    }

    // Minuit's estimated distance to the minimum of chi2, using the
    // Gauss-Newton Hessian: g^T H^-1 g with g the gradient of chi2/2.
    // Also keeps the undamped Gauss-Newton step for polish_step().
    double edm() {
        double e = 0.0;
        if (solve(0.0, gn_step_, e) < 0)
            return std::numeric_limits<double>::infinity();
        return e;
    }

    // Damped Gauss-Newton step (H + u D) delta = -g for the non-frozen
    // parameters, the predicted decrease of F for that step, and
    // bound_ = g^T (H + u D)^-1 g, which is at most the EDM because u D is
    // positive semi-definite.
    bool solve_step(double u) {
        predicted_ = 0.0;
        const int m = solve(u, delta_, bound_);
        if (m < 0)
            return false;
        for (int a = 0; a < nfree_; ++a) {
            if (active_[a])
                continue;
            predicted_ +=
                0.5 * delta_[a] *
                (u * std::max(jtj_(a, a), 1e-300) * delta_[a] - g_[a]);
        }
        return m == 0 || predicted_ > 0.0;
    }

    static constexpr double initial_damping = 0.1;
    // The two solves that bound the EDM differ by rounding only when the
    // damping is negligible; the margin keeps the stop decision exact.
    static constexpr double edm_margin = 1.0 + 1e-6;

    Options opt_;
    int nfree_ = 0;
    int accepted_ = 0; // trial points accepted in this fit
    std::array<int, NP> free_{};
    std::array<int, NP> free_pos_{};
    std::array<bool, NP> fixed_{};
    std::array<bool, NP> active_{};
    Vec<NP> lower_{};
    Vec<NP> upper_{};
    Vec<NP> p_{};     // all parameters at the last evaluated point
    Vec<NP> q_{};     // free parameters at the current point
    Vec<NP> q_new_{}; // trial point
    Vec<NP> q_alt_{}; // trial point truncated at the limits
    Vec<NP> g_{};
    Vec<NP> delta_{};
    Vec<NP> gn_step_{};
    Vec<NP> start_diag_{}; // Hessian diagonal at the start point
    SymMat<NP> jtj_;       // Hessian of the free parameters
    double gmax_ = 0.0;
    double predicted_ = 0.0;
    double bound_ = 0.0;
    Normal cur_;
    Normal trial_;
    Normal alt_;
};

} // namespace aare::detail
