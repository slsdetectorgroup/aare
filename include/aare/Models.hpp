// SPDX-License-Identifier: MPL-2.0
#pragma once

#include "aare/NDView.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <type_traits>
#include <vector>

namespace aare::model {

inline constexpr double inv_sqrt2 = 0.70710678118654752440;
inline constexpr double inv_sqrt_2pi = 0.39894228040143267794;

/**
 * @brief erf(z) given exp(-z^2), from the Abramowitz-Stegun approximation
 * 7.1.26 (Handbook of Mathematical Functions, max error about 1.5e-7).
 *
 * Models whose derivatives need exp(-z^2) anyway pass it in, so that a
 * single exponential serves both the value and the gradient.
 */
inline double fast_erf_from_exp(double z, double exp_minus_z2) {
    const double a1 = 0.254829592;
    const double a2 = -0.284496736;
    const double a3 = 1.421413741;
    const double a4 = -1.453152027;
    const double a5 = 1.061405429;
    const double p = 0.3275911;

    const double t = 1.0 / (1.0 + p * std::abs(z));
    const double y =
        1.0 -
        (((((a5 * t + a4) * t + a3) * t + a2) * t + a1) * t) * exp_minus_z2;

    return z < 0 ? -y : y;
}

/** @brief erf approximation, faster than std::erf; see fast_erf_from_exp. */
inline double fast_erf(double x) {
    return fast_erf_from_exp(x, std::exp(-x * x));
}

/**
 * @brief exp(x) without a call into libm, for loops that should vectorise.
 *
 * Range reduction x = k ln2 + r with the integer k taken from the mantissa
 * of x * log2(e) + 1.5 * 2^52, a degree-13 Taylor polynomial for exp(r) on
 * |r| <= ln2 / 2, and 2^k assembled in the exponent bits, so that a loop
 * calling it compiles to vector instructions on SSE2, AVX and NEON. The
 * relative error is below 4e-16 for finite x; |x| is clamped to 700. The
 * bulk basis functions of the models (basis_columns) use it; eval and
 * eval_and_grad keep std::exp.
 */
inline double fast_exp(double x) {
    constexpr double log2e = 1.4426950408889634074;
    constexpr double magic = 6755399441055744.0; // 1.5 * 2^52
    constexpr double ln2_hi = 0.693145751953125;
    constexpr double ln2_lo = 1.42860682030941723212e-6;
    x = std::clamp(x, -700.0, 700.0);
    const double t = x * log2e + magic;
    const double k = t - magic;
    const double r = (x - k * ln2_hi) - k * ln2_lo;
    double p = 1.0 / 6227020800.0;
    p = p * r + 1.0 / 479001600.0;
    p = p * r + 1.0 / 39916800.0;
    p = p * r + 1.0 / 3628800.0;
    p = p * r + 1.0 / 362880.0;
    p = p * r + 1.0 / 40320.0;
    p = p * r + 1.0 / 5040.0;
    p = p * r + 1.0 / 720.0;
    p = p * r + 1.0 / 120.0;
    p = p * r + 1.0 / 24.0;
    p = p * r + 1.0 / 6.0;
    p = p * r + 0.5;
    p = p * r + 1.0;
    p = p * r + 1.0;
    std::int64_t ti = 0;
    std::int64_t mi = 0;
    std::memcpy(&ti, &t, sizeof(ti));
    std::memcpy(&mi, &magic, sizeof(mi));
    const std::int64_t bits = ((ti - mi) + 1023) << 52;
    double scale = 0.0;
    std::memcpy(&scale, &bits, sizeof(scale));
    return p * scale;
}

/**
 * @brief Per-parameter metadata: name and optional default bounds.
 *
 * Used by FitModel for parameter names and default limits.
 * Unbounded directions use ±no_bound as sentinels.
 */
struct ParamInfo {
    const char *name;  // name of parameter
    double default_lo; // lower bound value
    double default_hi; // upper bound value
};

inline constexpr double no_bound = std::numeric_limits<double>::infinity();

/**
 * @brief Detects a separable model, one that declares its linear parameters
 * for Minimizer::VarPro.
 *
 * A model whose function is linear in some of its parameters lists them in
 * ascending order in `linear_par` and provides
 *
 *   static void basis_and_grad(double x, const std::vector<double> &par,
 *                              std::array<double, nlin> &phi,
 *                              std::array<std::array<double, nlin>, nnl> &dphi)
 *
 * with nlin = linear_par.size() and nnl = npar - nlin, such that
 *
 *   f(x; par) = sum_j par[linear_par[j]] * phi[j]
 *
 * and dphi[k][j] = d phi[j] / d par[nonlinear[k]], where nonlinear lists the
 * remaining parameter indices in ascending order (see separable_traits).
 * basis_and_grad ignores the linear entries of par. The [fit] tests check
 * phi and dphi against eval and eval_and_grad for every model.
 *
 * A model may additionally provide the same quantities for all scan points
 * at once, see has_basis_columns:
 *
 *   static void basis_columns(const double *x, ssize_t n,
 *                             const std::vector<double> &par,
 *                             double *phi, double *dphi)
 *
 * writes phi[j * n + i] = phi_j(x_i) and dphi[(k * nlin + j) * n + i] =
 * d phi_j / d par[nonlinear[k]] (x_i), one contiguous column per function.
 * Written as a loop free of reductions with the parameters hoisted and
 * fast_exp instead of std::exp, it compiles to vector instructions, which
 * is where VarPro spends most of its time. The tests check it
 * against basis_and_grad.
 */
template <typename Model, typename = void>
struct is_separable : std::false_type {};

template <typename Model>
struct is_separable<Model, std::void_t<decltype(Model::linear_par),
                                       decltype(&Model::basis_and_grad)>>
    : std::true_type {};

/** @brief Detects a separable model that provides basis_columns. */
template <typename Model, typename = void>
struct has_basis_columns : std::false_type {};

template <typename Model>
struct has_basis_columns<Model, std::void_t<decltype(&Model::basis_columns)>>
    : std::true_type {};

/** @brief The parameter indices not listed in linear, in ascending order. */
template <std::size_t Npar, std::size_t Nlin>
constexpr std::array<std::size_t, Npar - Nlin>
nonlinear_indices(const std::array<std::size_t, Nlin> &linear) {
    std::array<std::size_t, Npar - Nlin> out{};
    std::size_t m = 0;
    for (std::size_t k = 0; k < Npar; ++k) {
        bool is_linear = false;
        for (std::size_t j = 0; j < Nlin; ++j)
            is_linear = is_linear || linear[j] == k;
        if (!is_linear)
            out[m++] = k;
    }
    return out;
}

/** @brief Index bookkeeping of a separable model, see is_separable. */
template <typename Model> struct separable_traits {
    static constexpr std::size_t npar = Model::npar;
    static constexpr std::size_t nlin = Model::linear_par.size();
    static constexpr std::size_t nnl = npar - nlin;
    static constexpr std::array<std::size_t, nnl> nonlinear_par =
        nonlinear_indices<npar>(Model::linear_par);
    static constexpr std::size_t linear_index(int j) {
        return Model::linear_par[static_cast<std::size_t>(j)];
    }
    using Basis = std::array<double, nlin>;
    using BasisGrad = std::array<std::array<double, nlin>, nnl>;
};

// _____________________________________________________________________
//
// Pol1
// _____________________________________________________________________

/**
 * @brief Affine function (polynomial of degree 1).
 *
 * f(x) = p0 + p1 *x
 *
 * Parameters:
 *   par[0] = p0    (intercept)
 *   par[1] = p1    (slope)
 *
 * Analytic partial derivatives:
 *   df/dp0    = 1
 *   df/dp1    = x
 */
struct Pol1 {
    static constexpr std::size_t npar = 2;

    static constexpr std::array<ParamInfo, npar> param_info = {{
        {"p0", -no_bound, no_bound},
        {"p1", -no_bound, no_bound},
    }};

    static double eval(double x, const std::vector<double> &par) {
        return par[0] + par[1] * x;
    }

    static void eval_and_grad(double x, const std::vector<double> &par,
                              double &f, std::array<double, npar> &g) {
        f = par[0] + par[1] * x;

        g[0] = 1.0; // df/dp0
        g[1] = x;   // df/dp1
    }

    /** @brief Both parameters are linear: f = p0 * 1 + p1 * x. */
    static constexpr std::array<std::size_t, 2> linear_par = {0, 1};

    static void basis_and_grad(
        double x, [[maybe_unused]] const std::vector<double> &par,
        std::array<double, 2> &phi,
        [[maybe_unused]] std::array<std::array<double, 2>, 0> &dphi) {
        phi[0] = 1.0;
        phi[1] = x;
    }

    static bool is_valid([[maybe_unused]] const std::vector<double> &par) {
        return true; // always valid
    }

    /** @brief Estimate from endpoints: slope = dy/dx, intercept from first
     * point. */
    static std::array<double, npar> estimate_par(NDView<double, 1> x,
                                                 NDView<double, 1> y) {
        const double dx = x[x.size() - 1] - x[0];
        const double dy = y[y.size() - 1] - y[0];
        const double slope = (std::abs(dx) > 1e-12) ? dy / dx : 0.0;
        const double intercept = y[0] - slope * x[0];

        return {intercept, slope};
    }
};

// _____________________________________________________________________
//
// Pol2
// _____________________________________________________________________

/**
 * @brief Polynomial fuction of degree 2
 *
 * f(x) = p0 + p1 * x + p2 * x * x
 *
 * Parameters:
 *   par[0] = p0    (constant term)
 *   par[1] = p1    (linear coef)
 *   par[2] = p2    (quadratic coef)
 *
 * Analytic partial derivatives:
 *   df/dp0 = 1
 *   df/dp1 = x
 *   df/dp2 = x * x
 */
struct Pol2 {
    static constexpr std::size_t npar = 3;

    static constexpr std::array<ParamInfo, npar> param_info = {{
        {"p0", -no_bound, no_bound},
        {"p1", -no_bound, no_bound},
        {"p2", -no_bound, no_bound},
    }};

    static double eval(double x, const std::vector<double> &par) {
        return par[0] + par[1] * x + par[2] * x * x;
    }

    static void eval_and_grad(double x, const std::vector<double> &par,
                              double &f, std::array<double, npar> &g) {
        f = par[0] + par[1] * x + par[2] * x * x;

        g[0] = 1.0;   // df/dp0
        g[1] = x;     // df/dp1
        g[2] = x * x; // df/dp2
    }

    /** @brief All parameters are linear: f = p0 * 1 + p1 * x + p2 * x^2. */
    static constexpr std::array<std::size_t, 3> linear_par = {0, 1, 2};

    static void basis_and_grad(
        double x, [[maybe_unused]] const std::vector<double> &par,
        std::array<double, 3> &phi,
        [[maybe_unused]] std::array<std::array<double, 3>, 0> &dphi) {
        phi[0] = 1.0;
        phi[1] = x;
        phi[2] = x * x;
    }

    static bool is_valid([[maybe_unused]] const std::vector<double> &par) {
        return true; // always valid
    }

    /**
     * @brief Data-driven initial estimates for a degree-2 polynomial.
     *
     * For f(x) = p0 + p1*x + p2*x², the derivative is f'(x) = p1 + 2*p2*x.
     * Measuring the slope at two positions and subtracting eliminates p1:
     *
     *   f'(x_e) = p1 + 2*p2*x_e
     *   f'(x_s) = p1 + 2*p2*x_s
     *   ─────────────────────────
     *   f'(x_e) - f'(x_s) = 2*p2*(x_e - x_s)
     *
     *   => p2 = (slope_e - slope_s) / (2 * (x_e - x_s))
     *
     * Slopes are estimated over the first and last 10% of points to
     * average out noise.  p1 is back-solved from the start slope, and
     * p0 from the first data point.  Degrades to a linear estimate
     * when curvature is negligible i.e. slope_e ≈ slope_s -> p2 ≈ 0.
     */
    static std::array<double, npar> estimate_par(NDView<double, 1> x,
                                                 NDView<double, 1> y) {
        const ssize_t n = y.size();
        const ssize_t tail = std::max<ssize_t>(n / 10, 2);

        // start: slope from first 10%
        const double x_s = (x[0] + x[tail - 1]) * 0.5;
        const double slope_s = (y[tail - 1] - y[0]) / (x[tail - 1] - x[0]);

        // end: slope from last 10%
        const double x_e = (x[n - tail] + x[n - 1]) * 0.5;
        const double slope_e =
            (y[n - 1] - y[n - tail]) / (x[n - 1] - x[n - tail]);

        const double dx = x_e - x_s;
        const double p2 =
            (std::abs(dx) > 1e-12) ? (slope_e - slope_s) / (2.0 * dx) : 0.0;

        // p1 from slope_s = p1 + 2*p2*x_s
        const double p1 = slope_s - 2.0 * p2 * x_s;

        // p0 from first point: y[0] = p0 + p1*x[0] + p2*x[0]^2
        const double p0 = y[0] - p1 * x[0] - p2 * x[0] * x[0];

        return {p0, p1, p2};
    }
};

// _____________________________________________________________________
//
// Gaussian
// _____________________________________________________________________

/**
 * @brief Unnormalized Gaussian model.
 *
 * f(x) = A * exp( -(x - mu)^2 / (2 * sigma^2) )
 *
 * Parameters:
 *   par[0] = A     (amplitude)
 *   par[1] = mu    (mean / center)
 *   par[2] = sigma (standard deviation)
 *
 * Analytic partial derivatives:
 *   df/dA     = exp( -(x-mu)^2 / (2*sigma^2) )
 *   df/dmu    = A * exp(...) * (x - mu) / sigma^2
 *   df/dsigma = A * exp(...) * (x - mu)^2 / sigma^3
 */
struct Gaussian {
    static constexpr std::size_t npar = 3;

    static constexpr std::array<ParamInfo, npar> param_info = {{
        {"A", -no_bound, no_bound},
        {"mu", -no_bound, no_bound},
        {"sigma", 1e-12, no_bound},
    }};

    static double eval(double x, const std::vector<double> &par) {
        const double A = par[0];
        const double mu = par[1];
        const double sig = par[2];

        const double dx = x - mu;
        const double inv_2sig2 = 1.0 / (2.0 * sig * sig);
        return A * std::exp(-dx * dx * inv_2sig2);
    }

    static void eval_and_grad(double x, const std::vector<double> &par,
                              double &f, std::array<double, npar> &g) {
        const double A = par[0];
        const double mu = par[1];
        const double sig = par[2];

        const double dx = x - mu;
        const double inv_2sig2 = 1.0 / (2.0 * sig * sig);
        const double e = std::exp(-dx * dx * inv_2sig2);

        f = A * e;

        g[0] = e;                            // df/dA
        g[1] = 2.0 * A * e * dx * inv_2sig2; // df/dmu    = A*e*(x-mu)/sig^2
        g[2] = 2.0 * A * e * dx * dx * inv_2sig2 /
               sig; // df/dsigma = A*e*(x-mu)^2/sig^3
    }

    /** @brief A is linear: f = A * exp(-(x - mu)^2 / (2 sigma^2)). */
    static constexpr std::array<std::size_t, 1> linear_par = {0};

    static void basis_and_grad(double x, const std::vector<double> &par,
                               std::array<double, 1> &phi,
                               std::array<std::array<double, 1>, 2> &dphi) {
        const double mu = par[1];
        const double sig = par[2];

        const double dx = x - mu;
        const double inv_sig = 1.0 / sig;
        const double inv_sig2 = inv_sig * inv_sig;
        const double e = std::exp(-0.5 * dx * dx * inv_sig2);

        phi[0] = e;
        dphi[0][0] = e * dx * inv_sig2;                // d/dmu
        dphi[1][0] = e * dx * dx * inv_sig2 * inv_sig; // d/dsigma
    }

    /** @brief basis_and_grad for all points, one column per function. */
    static void basis_columns(const double *x, ssize_t n,
                              const std::vector<double> &par, double *phi,
                              double *dphi) {
        const double mu = par[1];
        const double inv_sig = 1.0 / par[2];
        const double inv_sig2 = inv_sig * inv_sig;
        double *e_col = phi;
        double *dmu = dphi;
        double *dsig = dphi + n;
        for (ssize_t i = 0; i < n; ++i) {
            const double dx = x[i] - mu;
            const double e = fast_exp(-0.5 * dx * dx * inv_sig2);
            e_col[i] = e;
            dmu[i] = e * dx * inv_sig2;
            dsig[i] = e * dx * dx * inv_sig2 * inv_sig;
        }
    }

    /** @brief Reject degenerate sigma (zero width). */
    static bool is_valid(const std::vector<double> &par) {
        return par[2] != 0.0;
    }

    /**
     * @brief Quick data-driven initial parameter estimates.
     *
     * A  = max(y)
     * mu = x at max(y)   (centroid of top 10% as refinement)
     * sigma = FWHM / 2.35
     */
    static std::array<double, npar> estimate_par(NDView<double, 1> x,
                                                 NDView<double, 1> y) {
        // find peak
        const auto max_it = std::max_element(y.begin(), y.end());
        const ssize_t i_max = std::distance(y.begin(), max_it);

        const double A = *max_it;
        const double mu = x[i_max];

        // FWHM estimate
        const double half = A * 0.5;
        double x_lo = mu, x_hi = mu;
        for (ssize_t i = i_max; i >= 0; --i)
            if (y[i] < half) {
                x_lo = x[i];
                break;
            }
        for (ssize_t i = i_max; i < y.size(); ++i)
            if (y[i] < half) {
                x_hi = x[i];
                break;
            }
        const double sig = std::max((x_hi - x_lo) / 2.35, 1e-6);

        return {A, mu, sig};
    }
};

// _____________________________________________________________________
//
// GaussianErfcPlateau
// _____________________________________________________________________

/**
 * @brief Gaussian peak plus a falling error-function plateau.
 *
 * Equivalent to the ROOT-style expression:
 *
 *   [0] * exp(-0.5 * ((x - [2])/[3])^2)
 * + [1] * (1 - erf((x - [2]) / ([3] * sqrt(2)))) / 2
 *
 * f(x) = A * exp(-(x - mu)^2 / (2*sigma^2))
 *      + S * 0.5 * (1 - erf((x - mu) / (sqrt(2)*sigma)))
 *
 * Parameters:
 *   par[0] = A      (Gaussian amplitude)
 *   par[1] = S      (left plateau height)
 *   par[2] = mu     (shared Gaussian center / step inflection point)
 *   par[3] = sigma  (shared Gaussian width / step width, must be > 0)
 *
 * Analytic partial derivatives:
 *   Let dx = x - mu
 *       z  = dx / (sqrt(2)*sigma)
 *       E  = exp(-z^2)
 *       H  = 0.5 * (1 - erf(z))
 *
 *   df/dA     = E
 *   df/dS     = H
 *   df/dmu    = A*E*dx/sigma^2 + S*E/(sqrt(2*pi)*sigma)
 *   df/dsigma = A*E*dx^2/sigma^3 + S*E*dx/(sqrt(2*pi)*sigma^2)
 */
struct GaussianErfcPlateau {
    static constexpr std::size_t npar = 4;

    static constexpr std::array<ParamInfo, npar> param_info = {{
        {"A", -no_bound, no_bound},
        {"S", -no_bound, no_bound},
        {"mu", -no_bound, no_bound},
        {"sigma", 1e-12, no_bound},
    }};

    static double eval(double x, const std::vector<double> &par) {
        const double A = par[0];
        const double S = par[1];
        const double mu = par[2];
        const double sig = par[3];

        const double dx = x - mu;
        const double z = dx * inv_sqrt2 / sig;

        const double e = std::exp(-z * z);
        const double step = 0.5 * (1.0 - fast_erf_from_exp(z, e));

        return A * e + S * step;
    }

    static void eval_and_grad(double x, const std::vector<double> &par,
                              double &f, std::array<double, npar> &g) {
        const double A = par[0];
        const double S = par[1];
        const double mu = par[2];
        const double sig = par[3];

        const double dx = x - mu;
        const double z = dx * inv_sqrt2 / sig;

        const double e = std::exp(-z * z);
        const double step = 0.5 * (1.0 - fast_erf_from_exp(z, e));

        f = A * e + S * step;

        const double inv_sig = 1.0 / sig;
        const double inv_sig2 = inv_sig * inv_sig;
        const double inv_sig3 = inv_sig2 * inv_sig;

        g[0] = e;    // df/dA
        g[1] = step; // df/dS

        g[2] = A * e * dx * inv_sig2 + S * inv_sqrt_2pi * e * inv_sig; // df/dmu

        g[3] = A * e * dx * dx * inv_sig3 +
               S * inv_sqrt_2pi * e * dx * inv_sig2; // df/dsigma
    }

    /** @brief A and S are linear: f = A * e + S * step. */
    static constexpr std::array<std::size_t, 2> linear_par = {0, 1};

    static void basis_and_grad(double x, const std::vector<double> &par,
                               std::array<double, 2> &phi,
                               std::array<std::array<double, 2>, 2> &dphi) {
        const double mu = par[2];
        const double sig = par[3];

        const double dx = x - mu;
        const double inv_sig = 1.0 / sig;
        const double inv_sig2 = inv_sig * inv_sig;
        const double inv_sig3 = inv_sig2 * inv_sig;
        const double z = dx * inv_sqrt2 * inv_sig;

        const double e = std::exp(-z * z);
        const double step = 0.5 * (1.0 - fast_erf_from_exp(z, e));

        phi[0] = e;
        phi[1] = step;
        dphi[0][0] = e * dx * inv_sig2;                // de/dmu
        dphi[0][1] = inv_sqrt_2pi * e * inv_sig;       // dstep/dmu
        dphi[1][0] = e * dx * dx * inv_sig3;           // de/dsigma
        dphi[1][1] = inv_sqrt_2pi * e * dx * inv_sig2; // dstep/dsigma
    }

    /** @brief basis_and_grad for all points, one column per function. */
    static void basis_columns(const double *x, ssize_t n,
                              const std::vector<double> &par, double *phi,
                              double *dphi) {
        const double mu = par[2];
        const double inv_sig = 1.0 / par[3];
        const double inv_sig2 = inv_sig * inv_sig;
        const double inv_sig3 = inv_sig2 * inv_sig;
        const double cz = inv_sqrt2 * inv_sig;
        const double ce = inv_sqrt_2pi * inv_sig;
        const double ce2 = inv_sqrt_2pi * inv_sig2;
        double *e_col = phi;
        double *step_col = phi + n;
        double *de_dmu = dphi;
        double *dstep_dmu = dphi + n;
        double *de_dsig = dphi + 2 * n;
        double *dstep_dsig = dphi + 3 * n;
        for (ssize_t i = 0; i < n; ++i) {
            const double dx = x[i] - mu;
            const double z = dx * cz;
            const double e = fast_exp(-z * z);
            const double step = 0.5 * (1.0 - fast_erf_from_exp(z, e));
            e_col[i] = e;
            step_col[i] = step;
            de_dmu[i] = e * dx * inv_sig2;
            dstep_dmu[i] = ce * e;
            de_dsig[i] = e * dx * dx * inv_sig3;
            dstep_dsig[i] = ce2 * e * dx;
        }
    }

    /** @brief Reject degenerate or negative width. */
    static bool is_valid(const std::vector<double> &par) {
        return par[3] > 0.0;
    }

    /**
     * @brief Data-driven initial parameter estimates.
     *
     * Assumes x is sorted ascending and the plateau is on the left.
     *
     * S      ~ average of first 10% of y values
     * mu     ~ x at maximum y
     * A      ~ y_max - 0.5*S
     * sigma  ~ right-side half-width of the peak, fallback to 10% x-range
     */
    static std::array<double, npar> estimate_par(NDView<double, 1> x,
                                                 NDView<double, 1> y) {
        const ssize_t n = y.size();

        const auto max_it = std::max_element(y.begin(), y.end());
        const ssize_t i_max = std::distance(y.begin(), max_it);

        const double y_max = *max_it;

        const double x_range = std::max(x[n - 1] - x[0], 1e-9);

        const ssize_t tail = std::min<ssize_t>(std::max<ssize_t>(n / 10, 2), n);

        double left_mean = 0.0;
        double right_mean = 0.0;

        for (ssize_t i = 0; i < tail; ++i)
            left_mean += y[i];
        left_mean /= tail;

        for (ssize_t i = n - tail; i < n; ++i)
            right_mean += y[i];
        right_mean /= tail;

        const double S = left_mean;
        const double mu = x[i_max];

        // At x = mu, the erfc step contributes roughly S/2.
        const double A = y_max - 0.5 * S;

        // Estimate sigma from the right half-width of the peak.
        // This avoids the left plateau biasing the FWHM estimate.
        const double half = right_mean + 0.5 * (y_max - right_mean);

        double sig = 0.1 * x_range;
        for (ssize_t i = i_max; i < n; ++i) {
            if (y[i] <= half) {
                const double half_width = std::abs(x[i] - mu);
                if (half_width > 1e-12) {
                    sig = half_width / 1.1774100225154747; // sqrt(2*ln(2))
                }
                break;
            }
        }

        sig = std::max(sig, 1e-6);

        return {A, S, mu, sig};
    }
};

// _____________________________________________________________________
//
// GaussianChargeSharing
// _____________________________________________________________________

/**
 * @brief Gaussian peak with erfc charge-sharing tail and linear pedestal.
 *
 * Equivalent to the old ROOT energyCalibration function:
 *
 *   gaussChargeSharing(x, par)
 *
 * for sign = +1:
 *
 *   f(x) = p0 - p1*x
 *        + N * [ G(x; mu, sigma) + C * H(x; mu, sigma) ]
 *
 * where:
 *
 *   G = exp(-0.5 * ((x - mu)/sigma)^2)
 *   H = 0.5 * erfc((x - mu)/(sqrt(2)*sigma))
 *
 * Parameters:
 *   par[0] = p0     background pedestal
 *   par[1] = p1     background slope, ROOT convention uses p0 - p1*x
 *   par[2] = mu     peak position
 *   par[3] = sigma  noise RMS / Gaussian width
 *   par[4] = N      number of photons / global amplitude
 *   par[5] = C      charge-sharing pedestal
 */
struct GaussianChargeSharing {
    static constexpr std::size_t npar = 6;

    static constexpr std::array<ParamInfo, npar> param_info = {{
        {"p0", -no_bound, no_bound},
        {"p1", -no_bound, no_bound},
        {"mu", -no_bound, no_bound},
        {"sigma", 1e-12, no_bound},
        {"N", -no_bound, no_bound},
        {"C", -no_bound, no_bound},
    }};

    static double eval(double x, const std::vector<double> &par) {
        const double p0 = par[0];
        const double p1 = par[1];
        const double mu = par[2];
        const double sig = par[3];
        const double N = par[4];
        const double C = par[5];

        const double dx = x - mu;
        const double u = dx / sig;

        const double G = std::exp(-0.5 * u * u);
        const double H = 0.5 * (1.0 - fast_erf_from_exp(u * inv_sqrt2, G));

        return p0 - p1 * x + N * (G + C * H);
    }

    static void eval_and_grad(double x, const std::vector<double> &par,
                              double &f, std::array<double, npar> &g) {
        const double p0 = par[0];
        const double p1 = par[1];
        const double mu = par[2];
        const double sig = par[3];
        const double N = par[4];
        const double C = par[5];

        const double dx = x - mu;
        const double u = dx / sig;

        const double G = std::exp(-0.5 * u * u);
        const double H = 0.5 * (1.0 - fast_erf_from_exp(u * inv_sqrt2, G));

        f = p0 - p1 * x + N * (G + C * H);

        const double inv_sig = 1.0 / sig;
        const double inv_sig2 = inv_sig * inv_sig;
        const double inv_sig3 = inv_sig2 * inv_sig;

        const double dG_dmu = G * dx * inv_sig2;
        const double dG_dsig = G * dx * dx * inv_sig3;

        const double dH_dmu = inv_sqrt_2pi * G * inv_sig;
        const double dH_dsig = inv_sqrt_2pi * G * dx * inv_sig2;

        g[0] = 1.0;
        g[1] = -x;
        g[2] = N * (dG_dmu + C * dH_dmu);
        g[3] = N * (dG_dsig + C * dH_dsig);
        g[4] = G + C * H;
        g[5] = N * H;
    }

    /**
     * @brief p0, p1 and N are linear: f = p0 * 1 + p1 * (-x) + N * (G + C H).
     * C multiplies N, so it stays a nonlinear parameter.
     */
    static constexpr std::array<std::size_t, 3> linear_par = {0, 1, 4};

    static void basis_and_grad(double x, const std::vector<double> &par,
                               std::array<double, 3> &phi,
                               std::array<std::array<double, 3>, 3> &dphi) {
        const double mu = par[2];
        const double sig = par[3];
        const double C = par[5];

        const double inv_sig = 1.0 / sig;
        const double inv_sig2 = inv_sig * inv_sig;
        const double inv_sig3 = inv_sig2 * inv_sig;

        const double dx = x - mu;
        const double u = dx * inv_sig;

        const double G = std::exp(-0.5 * u * u);
        const double H = 0.5 * (1.0 - fast_erf_from_exp(u * inv_sqrt2, G));

        const double dG_dmu = G * dx * inv_sig2;
        const double dG_dsig = G * dx * dx * inv_sig3;
        const double dH_dmu = inv_sqrt_2pi * G * inv_sig;
        const double dH_dsig = inv_sqrt_2pi * G * dx * inv_sig2;

        phi[0] = 1.0;
        phi[1] = -x;
        phi[2] = G + C * H;
        for (int k = 0; k < 3; ++k) {
            dphi[k][0] = 0.0;
            dphi[k][1] = 0.0;
        }
        dphi[0][2] = dG_dmu + C * dH_dmu;   // d/dmu
        dphi[1][2] = dG_dsig + C * dH_dsig; // d/dsigma
        dphi[2][2] = H;                     // d/dC
    }

    /** @brief basis_and_grad for all points, one column per function. */
    static void basis_columns(const double *x, ssize_t n,
                              const std::vector<double> &par, double *phi,
                              double *dphi) {
        const double mu = par[2];
        const double inv_sig = 1.0 / par[3];
        const double C = par[5];
        const double inv_sig2 = inv_sig * inv_sig;
        const double inv_sig3 = inv_sig2 * inv_sig;
        double *one = phi;
        double *minus_x = phi + n;
        double *shape = phi + 2 * n;
        // d phi_0 and d phi_1 vanish for every nonlinear parameter.
        std::fill(dphi, dphi + 2 * n, 0.0);
        std::fill(dphi + 3 * n, dphi + 5 * n, 0.0);
        std::fill(dphi + 6 * n, dphi + 8 * n, 0.0);
        double *d_mu = dphi + 2 * n;
        double *d_sig = dphi + 5 * n;
        double *d_C = dphi + 8 * n;
        for (ssize_t i = 0; i < n; ++i) {
            const double xi = x[i];
            const double dx = xi - mu;
            const double u = dx * inv_sig;
            const double G = fast_exp(-0.5 * u * u);
            const double H = 0.5 * (1.0 - fast_erf_from_exp(u * inv_sqrt2, G));
            const double dG_dmu = G * dx * inv_sig2;
            const double dG_dsig = G * dx * dx * inv_sig3;
            const double dH_dmu = inv_sqrt_2pi * G * inv_sig;
            const double dH_dsig = inv_sqrt_2pi * G * dx * inv_sig2;
            one[i] = 1.0;
            minus_x[i] = -xi;
            shape[i] = G + C * H;
            d_mu[i] = dG_dmu + C * dH_dmu;
            d_sig[i] = dG_dsig + C * dH_dsig;
            d_C[i] = H;
        }
    }

    static bool is_valid(const std::vector<double> &par) {
        return par[3] > 0.0;
    }

    static std::array<double, npar> estimate_par(NDView<double, 1> x,
                                                 NDView<double, 1> y) {
        const ssize_t n = y.size();

        const auto max_it = std::max_element(y.begin(), y.end());
        const ssize_t i_max = std::distance(y.begin(), max_it);

        const double x_range = std::max(x[n - 1] - x[0], 1e-9);

        const ssize_t tail = std::min<ssize_t>(std::max<ssize_t>(n / 10, 2), n);

        double left_mean = 0.0;
        double right_mean = 0.0;

        for (ssize_t i = 0; i < tail; ++i)
            left_mean += y[i];
        left_mean /= tail;

        for (ssize_t i = n - tail; i < n; ++i)
            right_mean += y[i];
        right_mean /= tail;

        const double p0 = right_mean;
        const double p1 = 0.0;
        const double mu = x[i_max];

        const double N = std::max(*max_it - right_mean, 1e-9);

        // Left plateau excess is roughly N*C.
        const double C = (left_mean - right_mean) / N;

        double sigma = 0.1 * x_range;
        const double half = right_mean + 0.5 * ((*max_it) - right_mean);

        for (ssize_t i = i_max; i < n; ++i) {
            if (y[i] <= half) {
                const double half_width = std::abs(x[i] - mu);
                if (half_width > 1e-12)
                    sigma = half_width / 1.1774100225154747; // sqrt(2*ln(2))
                break;
            }
        }

        sigma = std::max(sigma, 1e-6);

        return {p0, p1, mu, sigma, N, C};
    }
};

// _____________________________________________________________________
//
// GaussianChargeSharingKb
// _____________________________________________________________________

/**
 * @brief Double-Gaussian Kα/Kβ spectrum with erfc charge-sharing tails.
 *
 * Equivalent to the old ROOT energyCalibration function:
 *
 *   gaussChargeSharingKb(x, par)
 *
 * for sign = +1:
 *
 *   mu_b = r * mu
 *
 *   f(x) = p0 - p1*x
 *        + N * [
 *              G(x; mu, sigma)
 *            + C * H(x; mu, sigma)
 *            + q * G(x; mu_b, sigma)
 *            + q * C * H(x; mu_b, sigma)
 *          ]
 *
 * Parameters:
 *   par[0] = p0      background pedestal
 *   par[1] = p1      background slope, ROOT convention uses p0 - p1*x
 *   par[2] = mu      Kα peak position
 *   par[3] = sigma   shared width / noise RMS
 *   par[4] = N       global amplitude / number of photons
 *   par[5] = C       charge-sharing pedestal
 *   par[6] = r       Kβ mean ratio, mu_Kb = r * mu
 *   par[7] = q       Kβ fraction
 */
struct GaussianChargeSharingKb {
    static constexpr std::size_t npar = 8;

    static constexpr std::array<ParamInfo, npar> param_info = {{
        {"p0", -no_bound, no_bound},
        {"p1", -no_bound, no_bound},
        {"mu", -no_bound, no_bound},
        {"sigma", 1e-12, no_bound},
        {"N", -no_bound, no_bound},
        {"C", -no_bound, no_bound},
        {"kb_mean", -no_bound, no_bound},
        {"kb_frac", -no_bound, no_bound},
    }};

    static double eval(double x, const std::vector<double> &par) {
        const double p0 = par[0];
        const double p1 = par[1];
        const double mu = par[2];
        const double sig = par[3];
        const double N = par[4];
        const double C = par[5];
        const double r = par[6];
        const double q = par[7];

        const double mu_b = r * mu;

        const double dx = x - mu;
        const double dxb = x - mu_b;

        const double u = dx / sig;
        const double ub = dxb / sig;

        const double G = std::exp(-0.5 * u * u);
        const double Gb = std::exp(-0.5 * ub * ub);

        const double H = 0.5 * (1.0 - fast_erf_from_exp(u * inv_sqrt2, G));
        const double Hb = 0.5 * (1.0 - fast_erf_from_exp(ub * inv_sqrt2, Gb));

        const double ka = G + C * H;
        const double kb = Gb + C * Hb;

        return p0 - p1 * x + N * (ka + q * kb);
    }

    static void eval_and_grad(double x, const std::vector<double> &par,
                              double &f, std::array<double, npar> &g) {
        const double p0 = par[0];
        const double p1 = par[1];
        const double mu = par[2];
        const double sig = par[3];
        const double N = par[4];
        const double C = par[5];
        const double r = par[6];
        const double q = par[7];

        const double mu_b = r * mu;

        const double dx = x - mu;
        const double dxb = x - mu_b;

        const double u = dx / sig;
        const double ub = dxb / sig;

        const double G = std::exp(-0.5 * u * u);
        const double Gb = std::exp(-0.5 * ub * ub);

        const double H = 0.5 * (1.0 - fast_erf_from_exp(u * inv_sqrt2, G));
        const double Hb = 0.5 * (1.0 - fast_erf_from_exp(ub * inv_sqrt2, Gb));

        const double ka = G + C * H;
        const double kb = Gb + C * Hb;

        f = p0 - p1 * x + N * (ka + q * kb);

        const double inv_sig = 1.0 / sig;
        const double inv_sig2 = inv_sig * inv_sig;
        const double inv_sig3 = inv_sig2 * inv_sig;

        const double dG_dmu = G * dx * inv_sig2;
        const double dG_dsig = G * dx * dx * inv_sig3;
        const double dH_dmu = inv_sqrt_2pi * G * inv_sig;
        const double dH_dsig = inv_sqrt_2pi * G * dx * inv_sig2;

        const double dGb_dmu_b = Gb * dxb * inv_sig2;
        const double dGb_dsig = Gb * dxb * dxb * inv_sig3;
        const double dHb_dmu_b = inv_sqrt_2pi * Gb * inv_sig;
        const double dHb_dsig = inv_sqrt_2pi * Gb * dxb * inv_sig2;

        const double dka_dmu = dG_dmu + C * dH_dmu;
        const double dka_dsig = dG_dsig + C * dH_dsig;

        const double dkb_dmu_b = dGb_dmu_b + C * dHb_dmu_b;
        const double dkb_dsig = dGb_dsig + C * dHb_dsig;

        g[0] = 1.0;
        g[1] = -x;

        // mu_b = r * mu, so dmu_b/dmu = r
        g[2] = N * (dka_dmu + q * r * dkb_dmu_b);

        g[3] = N * (dka_dsig + q * dkb_dsig);

        g[4] = ka + q * kb;

        g[5] = N * (H + q * Hb);

        // mu_b = r * mu, so dmu_b/dr = mu
        g[6] = N * q * mu * dkb_dmu_b;

        g[7] = N * kb;
    }

    /**
     * @brief p0, p1 and N are linear: f = p0 * 1 + p1 * (-x) + N * (ka + q kb).
     * C, r and q multiply N or enter the peak shapes, so they stay nonlinear.
     */
    static constexpr std::array<std::size_t, 3> linear_par = {0, 1, 4};

    static void basis_and_grad(double x, const std::vector<double> &par,
                               std::array<double, 3> &phi,
                               std::array<std::array<double, 3>, 5> &dphi) {
        const double mu = par[2];
        const double sig = par[3];
        const double C = par[5];
        const double r = par[6];
        const double q = par[7];

        const double inv_sig = 1.0 / sig;
        const double inv_sig2 = inv_sig * inv_sig;
        const double inv_sig3 = inv_sig2 * inv_sig;

        const double mu_b = r * mu;

        const double dx = x - mu;
        const double dxb = x - mu_b;

        const double u = dx * inv_sig;
        const double ub = dxb * inv_sig;

        const double G = std::exp(-0.5 * u * u);
        const double Gb = std::exp(-0.5 * ub * ub);

        const double H = 0.5 * (1.0 - fast_erf_from_exp(u * inv_sqrt2, G));
        const double Hb = 0.5 * (1.0 - fast_erf_from_exp(ub * inv_sqrt2, Gb));

        const double ka = G + C * H;
        const double kb = Gb + C * Hb;

        const double dG_dmu = G * dx * inv_sig2;
        const double dG_dsig = G * dx * dx * inv_sig3;
        const double dH_dmu = inv_sqrt_2pi * G * inv_sig;
        const double dH_dsig = inv_sqrt_2pi * G * dx * inv_sig2;

        const double dGb_dmu_b = Gb * dxb * inv_sig2;
        const double dGb_dsig = Gb * dxb * dxb * inv_sig3;
        const double dHb_dmu_b = inv_sqrt_2pi * Gb * inv_sig;
        const double dHb_dsig = inv_sqrt_2pi * Gb * dxb * inv_sig2;

        const double dka_dmu = dG_dmu + C * dH_dmu;
        const double dka_dsig = dG_dsig + C * dH_dsig;

        const double dkb_dmu_b = dGb_dmu_b + C * dHb_dmu_b;
        const double dkb_dsig = dGb_dsig + C * dHb_dsig;

        phi[0] = 1.0;
        phi[1] = -x;
        phi[2] = ka + q * kb;
        for (int k = 0; k < 5; ++k) {
            dphi[k][0] = 0.0;
            dphi[k][1] = 0.0;
        }
        dphi[0][2] = dka_dmu + q * r * dkb_dmu_b; // d/dmu, mu_b = r * mu
        dphi[1][2] = dka_dsig + q * dkb_dsig;     // d/dsigma
        dphi[2][2] = H + q * Hb;                  // d/dC
        dphi[3][2] = q * mu * dkb_dmu_b;          // d/dr
        dphi[4][2] = kb;                          // d/dq
    }

    /** @brief basis_and_grad for all points, one column per function. */
    static void basis_columns(const double *x, ssize_t n,
                              const std::vector<double> &par, double *phi,
                              double *dphi) {
        const double mu = par[2];
        const double inv_sig = 1.0 / par[3];
        const double C = par[5];
        const double r = par[6];
        const double q = par[7];
        const double inv_sig2 = inv_sig * inv_sig;
        const double inv_sig3 = inv_sig2 * inv_sig;
        const double mu_b = r * mu;
        double *one = phi;
        double *minus_x = phi + n;
        double *shape = phi + 2 * n;
        // d phi_0 and d phi_1 vanish for every nonlinear parameter; the
        // derivative of phi_2 with respect to nonlinear parameter k is
        // column k * 3 + 2.
        for (int k = 0; k < 5; ++k)
            std::fill(dphi + (k * 3) * n, dphi + (k * 3 + 2) * n, 0.0);
        double *d_mu = dphi + 2 * n;
        double *d_sig = dphi + 5 * n;
        double *d_C = dphi + 8 * n;
        double *d_r = dphi + 11 * n;
        double *d_q = dphi + 14 * n;
        for (ssize_t i = 0; i < n; ++i) {
            const double xi = x[i];
            const double dx = xi - mu;
            const double dxb = xi - mu_b;
            const double u = dx * inv_sig;
            const double ub = dxb * inv_sig;
            const double G = fast_exp(-0.5 * u * u);
            const double Gb = fast_exp(-0.5 * ub * ub);
            const double H = 0.5 * (1.0 - fast_erf_from_exp(u * inv_sqrt2, G));
            const double Hb =
                0.5 * (1.0 - fast_erf_from_exp(ub * inv_sqrt2, Gb));
            const double ka = G + C * H;
            const double kb = Gb + C * Hb;
            const double dG_dmu = G * dx * inv_sig2;
            const double dG_dsig = G * dx * dx * inv_sig3;
            const double dH_dmu = inv_sqrt_2pi * G * inv_sig;
            const double dH_dsig = inv_sqrt_2pi * G * dx * inv_sig2;
            const double dGb_dmu_b = Gb * dxb * inv_sig2;
            const double dGb_dsig = Gb * dxb * dxb * inv_sig3;
            const double dHb_dmu_b = inv_sqrt_2pi * Gb * inv_sig;
            const double dHb_dsig = inv_sqrt_2pi * Gb * dxb * inv_sig2;
            const double dka_dmu = dG_dmu + C * dH_dmu;
            const double dka_dsig = dG_dsig + C * dH_dsig;
            const double dkb_dmu_b = dGb_dmu_b + C * dHb_dmu_b;
            const double dkb_dsig = dGb_dsig + C * dHb_dsig;
            one[i] = 1.0;
            minus_x[i] = -xi;
            shape[i] = ka + q * kb;
            d_mu[i] = dka_dmu + q * r * dkb_dmu_b;
            d_sig[i] = dka_dsig + q * dkb_dsig;
            d_C[i] = H + q * Hb;
            d_r[i] = q * mu * dkb_dmu_b;
            d_q[i] = kb;
        }
    }

    static bool is_valid(const std::vector<double> &par) {
        return par[3] > 0.0;
    }

    static std::array<double, npar> estimate_par(NDView<double, 1> x,
                                                 NDView<double, 1> y) {
        const auto base = GaussianChargeSharing::estimate_par(x, y);

        const double p0 = base[0];
        const double p1 = base[1];
        const double mu = base[2];
        const double sigma = base[3];
        const double N = base[4];
        const double C = base[5];

        // Good generic starting values for Cu Kβ relative to Kα.
        const double kb_mean = 1.10;
        const double kb_frac = 0.10;

        return {p0, p1, mu, sigma, N, C, kb_mean, kb_frac};
    }
};

// _____________________________________________________________________
//
// RisingScurve
// _____________________________________________________________________

/**
 * @brief Rising S-curve (error-function step) with linear baseline and
 *        post-step slope.
 *
 * f(x) = (p0 + p1*x) + 0.5*(1 + erf((x - mu) / (sqrt(2)*sigma))) * (A + C*(x -
 * mu)))
 *
 * Parameters:
 *   par[0] = p0  (baseline offset)
 *   par[1] = p1  (baseline slope)
 *   par[2] = mu  (inflection point)
 *   par[3] = sigma  (transition width, must be > 0)
 *   par[4] = A  (step amplitude)
 *   par[5] = C  (post-step slope)
 */
struct RisingScurve {
    static constexpr std::size_t npar = 6;

    static constexpr std::array<ParamInfo, npar> param_info = {{
        {"p0", -no_bound, no_bound},
        {"p1", -no_bound, no_bound},
        {"mu", -no_bound, no_bound},
        {"sigma", 1e-12, no_bound},
        {"A", -no_bound, no_bound},
        {"C", -no_bound, no_bound},
    }};

    static double eval(double x, const std::vector<double> &par) {
        const double p0 = par[0];
        const double p1 = par[1];
        const double p2 = par[2];
        const double p3 = par[3];
        const double p4 = par[4];
        const double p5 = par[5];

        const double dx = x - p2;
        const double step = 0.5 * (1.0 + fast_erf(dx * inv_sqrt2 / p3));
        return (p0 + p1 * x) + step * (p4 + p5 * dx);
    }

    /**
     * @brief Evaluate function value and partial derivatives in a single pass.
     *
     * Uses:
     *   S(x)  = 0.5*(1 + erf(z)),  z = (x-p2) / (sqrt(2)*p3)
     *   dS/dp2 = -(1/sqrt(2*pi)) * exp(-z^2) / p3
     *   dS/dp3 = -(1/sqrt(2*pi)) * exp(-z^2) * (x-p2) / p3^2
     */
    static void eval_and_grad(double x, const std::vector<double> &par,
                              double &f, std::array<double, npar> &g) {
        const double p0 = par[0];
        const double p1 = par[1];
        const double p2 = par[2];
        const double p3 = par[3];
        const double p4 = par[4];
        const double p5 = par[5];

        const double dx = x - p2;
        const double z = dx * inv_sqrt2 / p3;
        const double e = std::exp(-z * z);
        const double step = 0.5 * (1.0 + fast_erf_from_exp(z, e));
        const double amp = p4 + p5 * dx;

        f = (p0 + p1 * x) + step * amp;

        const double dSdp2 = -inv_sqrt_2pi * e / p3;
        const double dSdp3 = -inv_sqrt_2pi * e * dx / (p3 * p3);

        g[0] = 1.0;                     // df/dp0
        g[1] = x;                       // df/dp1
        g[2] = dSdp2 * amp - step * p5; // df/dp2
        g[3] = dSdp3 * amp;             // df/dp3
        g[4] = step;                    // df/dp4
        g[5] = step * dx;               // df/dp5
    }

    /**
     * @brief p0, p1, A and C are linear:
     * f = p0 * 1 + p1 * x + A * S + C * S * (x - mu).
     */
    static constexpr std::array<std::size_t, 4> linear_par = {0, 1, 4, 5};

    static void basis_and_grad(double x, const std::vector<double> &par,
                               std::array<double, 4> &phi,
                               std::array<std::array<double, 4>, 2> &dphi) {
        const double p2 = par[2];
        const double p3 = par[3];

        const double inv_p3 = 1.0 / p3;
        const double dx = x - p2;
        const double z = dx * inv_sqrt2 * inv_p3;
        const double e = std::exp(-z * z);
        const double step = 0.5 * (1.0 + fast_erf_from_exp(z, e));

        const double dSdp2 = -inv_sqrt_2pi * e * inv_p3;
        const double dSdp3 = dSdp2 * dx * inv_p3;

        phi[0] = 1.0;
        phi[1] = x;
        phi[2] = step;
        phi[3] = step * dx;
        dphi[0][0] = 0.0;
        dphi[0][1] = 0.0;
        dphi[0][2] = dSdp2;             // dS/dmu
        dphi[0][3] = dSdp2 * dx - step; // d(S dx)/dmu
        dphi[1][0] = 0.0;
        dphi[1][1] = 0.0;
        dphi[1][2] = dSdp3;      // dS/dsigma
        dphi[1][3] = dSdp3 * dx; // d(S dx)/dsigma
    }

    /** @brief basis_and_grad for all points, one column per function. */
    static void basis_columns(const double *x, ssize_t n,
                              const std::vector<double> &par, double *phi,
                              double *dphi) {
        const double p2 = par[2];
        const double inv_p3 = 1.0 / par[3];
        const double cz = inv_sqrt2 * inv_p3;
        const double ce = -inv_sqrt_2pi * inv_p3;
        double *one = phi;
        double *x_col = phi + n;
        double *step_col = phi + 2 * n;
        double *step_dx = phi + 3 * n;
        // d phi_0 and d phi_1 vanish for mu and sigma.
        std::fill(dphi, dphi + 2 * n, 0.0);
        std::fill(dphi + 4 * n, dphi + 6 * n, 0.0);
        double *dmu_step = dphi + 2 * n;
        double *dmu_step_dx = dphi + 3 * n;
        double *dsig_step = dphi + 6 * n;
        double *dsig_step_dx = dphi + 7 * n;
        for (ssize_t i = 0; i < n; ++i) {
            const double xi = x[i];
            const double dx = xi - p2;
            const double z = dx * cz;
            const double e = fast_exp(-z * z);
            const double step = 0.5 * (1.0 + fast_erf_from_exp(z, e));
            const double dSdp2 = ce * e;
            const double dSdp3 = dSdp2 * dx * inv_p3;
            one[i] = 1.0;
            x_col[i] = xi;
            step_col[i] = step;
            step_dx[i] = step * dx;
            dmu_step[i] = dSdp2;
            dmu_step_dx[i] = dSdp2 * dx - step;
            dsig_step[i] = dSdp3;
            dsig_step_dx[i] = dSdp3 * dx;
        }
    }

    /** @brief Reject degenerate width (zero transition width). */
    static bool is_valid(const std::vector<double> &par) {
        return par[3] != 0.0;
    }

    /** @brief Data-driven initial parameter estimates for a rising S-curve. */
    static std::array<double, npar> estimate_par(NDView<double, 1> x,
                                                 NDView<double, 1> y) {
        const ssize_t n = y.size();

        // baseline: average of first ~10% of points (before turn-on)
        ssize_t n_base = std::max<ssize_t>(n / 10, 2);
        double sum_y = 0, sum_xy = 0, sum_x = 0, sum_x2 = 0;
        for (ssize_t i = 0; i < n_base; ++i) {
            sum_y += y[i];
            sum_x += x[i];
            sum_xy += x[i] * y[i];
            sum_x2 += x[i] * x[i];
        }
        double denom = n_base * sum_x2 - sum_x * sum_x;
        double p1 = (std::abs(denom) > 1e-30)
                        ? (n_base * sum_xy - sum_x * sum_y) / denom
                        : 0.0;
        double p0 = (sum_y - p1 * sum_x) / n_base;

        // plateau: average of last ~10%
        double plateau = 0;
        ssize_t n_plat = std::max<ssize_t>(n / 10, 2);
        for (ssize_t i = n - n_plat; i < n; ++i)
            plateau += y[i];
        plateau /= n_plat;

        // amplitude: plateau minus baseline at midpoint
        double x_mid = 0.5 * (x[0] + x[n - 1]);
        double baseline_at_mid = p0 + p1 * x_mid;
        double p4 = plateau - baseline_at_mid;

        // threshold: x where y first crosses 50% between baseline and plateau
        double y_half = baseline_at_mid + 0.5 * p4;
        double p2 = x_mid; // fallback
        for (ssize_t i = 0; i < n; ++i) {
            if (y[i] >= y_half) {
                p2 = x[i];
                break;
            }
        }

        // sigma: estimate from transition width (10%-90% rise)
        double y_10 = baseline_at_mid + 0.1 * p4;
        double y_90 = baseline_at_mid + 0.9 * p4;
        double x_10 = x[0], x_90 = x[n - 1];
        for (ssize_t i = 0; i < n; ++i) {
            if (y[i] >= y_10) {
                x_10 = x[i];
                break;
            }
        }
        for (ssize_t i = 0; i < n; ++i) {
            if (y[i] >= y_90) {
                x_90 = x[i];
                break;
            }
        }
        // for a Gaussian CDF: 10%-90% width = 2 * 1.2816 * sigma
        double p3 = std::max((x_90 - x_10) / 2.5631, 1.0);

        double p5 = 0.0; // assume flat gain, let optimizer find the slope

        return {p0, p1, p2, p3, p4, p5};
    }
};

// _____________________________________________________________________
//
// FallingScurve
// _____________________________________________________________________

/**
 * @brief Falling S-curve (complementary error-function step) with linear
 *        baseline and post-step slope.
 *
 * f(x) = (p0 + p1*x) + 0.5*(1 - erf((x - mu) / (sqrt(2)*sigma))) * (A + C*(x -
 * mu))
 *
 * Parameters are identical to RisingScurve.  The only difference is the
 * sign of the erf term, which flips the step direction (and the signs of
 * dS/dp2 and dS/dp3 in the gradient).
 */
struct FallingScurve {
    static constexpr std::size_t npar = 6;

    static constexpr std::array<ParamInfo, npar> param_info = {{
        {"p0", -no_bound, no_bound},
        {"p1", -no_bound, no_bound},
        {"mu", -no_bound, no_bound},
        {"sigma", 1e-12, no_bound},
        {"A", -no_bound, no_bound},
        {"C", -no_bound, no_bound},
    }};

    static double eval(double x, const std::vector<double> &par) {
        const double p0 = par[0];
        const double p1 = par[1];
        const double p2 = par[2];
        const double p3 = par[3];
        const double p4 = par[4];
        const double p5 = par[5];

        const double dx = x - p2;
        const double step = 0.5 * (1.0 - fast_erf(dx * inv_sqrt2 / p3));
        return (p0 + p1 * x) + step * (p4 + p5 * dx);
    }

    static void eval_and_grad(double x, const std::vector<double> &par,
                              double &f, std::array<double, npar> &g) {
        const double p0 = par[0];
        const double p1 = par[1];
        const double p2 = par[2];
        const double p3 = par[3];
        const double p4 = par[4];
        const double p5 = par[5];

        const double dx = x - p2;
        const double z = dx * inv_sqrt2 / p3;
        const double e = std::exp(-z * z);
        const double step = 0.5 * (1.0 - fast_erf_from_exp(z, e));
        const double amp = p4 + p5 * dx;

        f = (p0 + p1 * x) + step * amp;

        const double dSdp2 = +inv_sqrt_2pi * e / p3; // sign flipped vs rising
        const double dSdp3 = +inv_sqrt_2pi * e * dx / (p3 * p3);

        g[0] = 1.0;
        g[1] = x;
        g[2] = dSdp2 * amp - step * p5;
        g[3] = dSdp3 * amp;
        g[4] = step;
        g[5] = step * dx;
    }

    /**
     * @brief p0, p1, A and C are linear:
     * f = p0 * 1 + p1 * x + A * S + C * S * (x - mu).
     */
    static constexpr std::array<std::size_t, 4> linear_par = {0, 1, 4, 5};

    static void basis_and_grad(double x, const std::vector<double> &par,
                               std::array<double, 4> &phi,
                               std::array<std::array<double, 4>, 2> &dphi) {
        const double p2 = par[2];
        const double p3 = par[3];

        const double inv_p3 = 1.0 / p3;
        const double dx = x - p2;
        const double z = dx * inv_sqrt2 * inv_p3;
        const double e = std::exp(-z * z);
        const double step = 0.5 * (1.0 - fast_erf_from_exp(z, e));

        const double dSdp2 =
            inv_sqrt_2pi * e * inv_p3; // sign flipped vs rising
        const double dSdp3 = dSdp2 * dx * inv_p3;

        phi[0] = 1.0;
        phi[1] = x;
        phi[2] = step;
        phi[3] = step * dx;
        dphi[0][0] = 0.0;
        dphi[0][1] = 0.0;
        dphi[0][2] = dSdp2;             // dS/dmu
        dphi[0][3] = dSdp2 * dx - step; // d(S dx)/dmu
        dphi[1][0] = 0.0;
        dphi[1][1] = 0.0;
        dphi[1][2] = dSdp3;      // dS/dsigma
        dphi[1][3] = dSdp3 * dx; // d(S dx)/dsigma
    }

    /** @brief basis_and_grad for all points, one column per function. */
    static void basis_columns(const double *x, ssize_t n,
                              const std::vector<double> &par, double *phi,
                              double *dphi) {
        const double p2 = par[2];
        const double inv_p3 = 1.0 / par[3];
        const double cz = inv_sqrt2 * inv_p3;
        const double ce = inv_sqrt_2pi * inv_p3; // sign flipped vs rising
        double *one = phi;
        double *x_col = phi + n;
        double *step_col = phi + 2 * n;
        double *step_dx = phi + 3 * n;
        // d phi_0 and d phi_1 vanish for mu and sigma.
        std::fill(dphi, dphi + 2 * n, 0.0);
        std::fill(dphi + 4 * n, dphi + 6 * n, 0.0);
        double *dmu_step = dphi + 2 * n;
        double *dmu_step_dx = dphi + 3 * n;
        double *dsig_step = dphi + 6 * n;
        double *dsig_step_dx = dphi + 7 * n;
        for (ssize_t i = 0; i < n; ++i) {
            const double xi = x[i];
            const double dx = xi - p2;
            const double z = dx * cz;
            const double e = fast_exp(-z * z);
            const double step = 0.5 * (1.0 - fast_erf_from_exp(z, e));
            const double dSdp2 = ce * e;
            const double dSdp3 = dSdp2 * dx * inv_p3;
            one[i] = 1.0;
            x_col[i] = xi;
            step_col[i] = step;
            step_dx[i] = step * dx;
            dmu_step[i] = dSdp2;
            dmu_step_dx[i] = dSdp2 * dx - step;
            dsig_step[i] = dSdp3;
            dsig_step_dx[i] = dSdp3 * dx;
        }
    }

    /** @brief Reject degenerate width (zero transition width). */
    static bool is_valid(const std::vector<double> &par) {
        return par[3] != 0.0;
    }

    /** @brief Data-driven initial parameter estimates for a falling S-curve. */
    static std::array<double, npar> estimate_par(NDView<double, 1> x,
                                                 NDView<double, 1> y) {
        const ssize_t n = y.size();

        // baseline: last ~10% of points (after turn-off)
        ssize_t n_base = std::max<ssize_t>(n / 10, 2);
        double sum_y = 0, sum_xy = 0, sum_x = 0, sum_x2 = 0;
        for (ssize_t i = n - n_base; i < n; ++i) {
            sum_y += y[i];
            sum_x += x[i];
            sum_xy += x[i] * y[i];
            sum_x2 += x[i] * x[i];
        }
        double denom = n_base * sum_x2 - sum_x * sum_x;
        double p1 = (std::abs(denom) > 1e-30)
                        ? (n_base * sum_xy - sum_x * sum_y) / denom
                        : 0.0;
        double p0 = (sum_y - p1 * sum_x) / n_base;

        // plateau: average of first ~10%
        double plateau = 0;
        ssize_t n_plat = std::max<ssize_t>(n / 10, 2);
        for (ssize_t i = 0; i < n_plat; ++i)
            plateau += y[i];
        plateau /= n_plat;

        // amplitude: plateau minus baseline at midpoint
        double x_mid = 0.5 * (x[0] + x[n - 1]);
        double baseline_at_mid = p0 + p1 * x_mid;
        double p4 = plateau - baseline_at_mid;

        // threshold: x where y first drops below 50%
        double y_half = baseline_at_mid + 0.5 * p4;
        double p2 = x_mid; // fallback
        for (ssize_t i = 0; i < n; ++i) {
            if (y[i] <= y_half) {
                p2 = x[i];
                break;
            }
        }

        // sigma: estimate from transition width (90%-10% fall)
        double y_90 = baseline_at_mid + 0.9 * p4;
        double y_10 = baseline_at_mid + 0.1 * p4;
        double x_90 = x[0], x_10 = x[n - 1];
        for (ssize_t i = 0; i < n; ++i) {
            if (y[i] <= y_90) {
                x_90 = x[i];
                break;
            }
        }
        for (ssize_t i = 0; i < n; ++i) {
            if (y[i] <= y_10) {
                x_10 = x[i];
                break;
            }
        }
        // same CDF relationship: 10%-90% width = 2 * 1.2816 * sigma
        double p3 = std::max((x_10 - x_90) / 2.5631, 1.0);

        double p5 = 0.0;

        return {p0, p1, p2, p3, p4, p5};
    }
};

} // namespace aare::model
