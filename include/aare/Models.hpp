// SPDX-License-Identifier: MPL-2.0
#pragma once

#include "aare/NDView.hpp"
#include "aare/defs.hpp"
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
ALWAYS_INLINE double fast_erf_from_exp(double z, double exp_minus_z2) {
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
ALWAYS_INLINE double fast_erf(double x) {
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
 * eval_and_grad keep std::exp. It is ALWAYS_INLINE because gcc otherwise
 * leaves it out of line in the large fitting translation unit, and a call
 * per point defeats the vectorisation of the caller.
 */
ALWAYS_INLINE double fast_exp(double x) {
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
 * ascending order in `linear_par` and provides its basis functions and their
 * derivatives for all scan points at once:
 *
 *   static void basis_columns(const double *__restrict x, ssize_t n,
 *                             const std::vector<double> &par,
 *                             double *__restrict phi,
 *                             double *__restrict dphi)
 *
 * With nlin = linear_par.size() and nnl = npar - nlin it writes the nlin
 * columns phi[j * n + i] = phi_j(x_i) of the basis functions, such that
 *
 *   f(x; par) = sum_j par[linear_par[j]] * phi_j(x)
 *
 * and the nnl * nlin columns dphi[(k * nlin + j) * n + i] =
 * d phi_j / d par[nonlinear[k]] (x_i), where nonlinear lists the remaining
 * parameter indices in ascending order (see separable_traits). It ignores
 * the linear entries of par. The [fit] tests check the columns against eval
 * and eval_and_grad for every model.
 *
 * VarPro spends most of its time here, so the function is written to compile
 * to vector instructions: one loop free of reductions, with the parameters
 * hoisted and fast_exp instead of std::exp. The __restrict qualifiers are
 * needed: without them gcc has to prove that the output columns do not
 * overlap, exceeds its budget of run-time alias checks once a loop writes
 * more than three columns, and quietly emits scalar code.
 */
template <typename Model, typename = void>
struct is_separable : std::false_type {};

template <typename Model>
struct is_separable<Model, std::void_t<decltype(Model::linear_par),
                                       decltype(&Model::basis_columns)>>
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
};

/**
 * @brief Features of a peak on a background that may be higher on its left,
 * as the start estimates of the peak models need them.
 */
struct PeakEstimate {
    double mu;         // x at the maximum of y
    double y_max;      // maximum of y
    double left_mean;  // mean of the first ~10% of y
    double right_mean; // mean of the last ~10% of y
    double sigma;      // width of the peak
};

/**
 * @brief Estimate the features of a peak. Assumes x is sorted ascending.
 *
 * sigma comes from the half width on the right side of the peak, above the
 * right background, so that a plateau on the left does not bias it. It falls
 * back to 10% of the x range.
 */
inline PeakEstimate estimate_peak(NDView<double, 1> x, NDView<double, 1> y) {
    const ssize_t n = y.size();

    const auto max_it = std::max_element(y.begin(), y.end());
    const ssize_t i_max = std::distance(y.begin(), max_it);
    const double y_max = *max_it;
    const double mu = x[i_max];

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

    const double half = right_mean + 0.5 * (y_max - right_mean);

    double sigma = 0.1 * x_range;
    for (ssize_t i = i_max; i < n; ++i) {
        if (y[i] <= half) {
            const double half_width = std::abs(x[i] - mu);
            if (half_width > 1e-12)
                sigma = half_width / 1.1774100225154747; // sqrt(2*ln(2))
            break;
        }
    }
    sigma = std::max(sigma, 1e-6);

    return {mu, y_max, left_mean, right_mean, sigma};
}

/**
 * @brief A Gaussian peak G and the erfc step H below it at one point, with
 * their derivatives with respect to the peak position and width:
 *
 *   G = exp(-u^2 / 2),  H = erfc(u / sqrt(2)) / 2,  u = (x - mu) / sigma
 *
 *   dG/dmu    = G * u / sigma
 *   dG/dsigma = G * u^2 / sigma
 *   dH/dmu    = G / (sqrt(2*pi) * sigma)
 *   dH/dsigma = G * u / (sqrt(2*pi) * sigma)
 */
struct PeakTerms {
    double G;
    double H;
    double dG_dmu;
    double dG_dsig;
    double dH_dmu;
    double dH_dsig;
};

/**
 * @brief The terms of a peak from its Gaussian G = exp(-u^2 / 2) at
 * u = (x - mu) / sigma, shared by the peak models.
 *
 * The caller evaluates the exponential: with std::exp at a single point, and
 * with fast_exp in a loop that should vectorise. A model with two peaks
 * evaluates both exponentials before it asks for the terms, which then stay
 * in registers rather than being spilled around the second call of std::exp.
 * The function is inlined, so the terms a caller does not use cost nothing.
 */
ALWAYS_INLINE PeakTerms peak_terms(double G, double u, double inv_sig) {
    PeakTerms t;
    t.G = G;
    t.H = 0.5 * (1.0 - fast_erf_from_exp(u * inv_sqrt2, G));
    t.dG_dmu = G * u * inv_sig;
    t.dG_dsig = t.dG_dmu * u;
    t.dH_dmu = inv_sqrt_2pi * G * inv_sig;
    t.dH_dsig = t.dH_dmu * u;
    return t;
}

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

    static void basis_columns(const double *__restrict x, ssize_t n,
                              [[maybe_unused]] const std::vector<double> &par,
                              double *__restrict phi,
                              [[maybe_unused]] double *__restrict dphi) {
        double *one = phi;
        double *x_col = phi + n;
        for (ssize_t i = 0; i < n; ++i) {
            one[i] = 1.0;
            x_col[i] = x[i];
        }
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

    static void basis_columns(const double *__restrict x, ssize_t n,
                              [[maybe_unused]] const std::vector<double> &par,
                              double *__restrict phi,
                              [[maybe_unused]] double *__restrict dphi) {
        double *one = phi;
        double *x_col = phi + n;
        double *x2_col = phi + 2 * n;
        for (ssize_t i = 0; i < n; ++i) {
            one[i] = 1.0;
            x_col[i] = x[i];
            x2_col[i] = x[i] * x[i];
        }
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

    static void basis_columns(const double *__restrict x, ssize_t n,
                              const std::vector<double> &par,
                              double *__restrict phi, double *__restrict dphi) {
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
 * Analytic partial derivatives, with the peak G and the step H of PeakTerms:
 *   df/dA     = G
 *   df/dS     = H
 *   df/dmu    = A * dG/dmu + S * dH/dmu
 *   df/dsigma = A * dG/dsigma + S * dH/dsigma
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

        const double inv_sig = 1.0 / sig;
        const double u = (x - mu) * inv_sig;
        const PeakTerms t = peak_terms(std::exp(-0.5 * u * u), u, inv_sig);
        return A * t.G + S * t.H;
    }

    static void eval_and_grad(double x, const std::vector<double> &par,
                              double &f, std::array<double, npar> &g) {
        const double A = par[0];
        const double S = par[1];
        const double mu = par[2];
        const double sig = par[3];

        const double inv_sig = 1.0 / sig;
        const double u = (x - mu) * inv_sig;
        const PeakTerms t = peak_terms(std::exp(-0.5 * u * u), u, inv_sig);

        f = A * t.G + S * t.H;

        g[0] = t.G;                           // df/dA
        g[1] = t.H;                           // df/dS
        g[2] = A * t.dG_dmu + S * t.dH_dmu;   // df/dmu
        g[3] = A * t.dG_dsig + S * t.dH_dsig; // df/dsigma
    }

    /** @brief A and S are linear: f = A * G + S * H. */
    static constexpr std::array<std::size_t, 2> linear_par = {0, 1};

    static void basis_columns(const double *__restrict x, ssize_t n,
                              const std::vector<double> &par,
                              double *__restrict phi, double *__restrict dphi) {
        const double mu = par[2];
        const double inv_sig = 1.0 / par[3];
        double *G = phi;
        double *H = phi + n;
        double *dG_dmu = dphi;
        double *dH_dmu = dphi + n;
        double *dG_dsig = dphi + 2 * n;
        double *dH_dsig = dphi + 3 * n;
        for (ssize_t i = 0; i < n; ++i) {
            const double u = (x[i] - mu) * inv_sig;
            const PeakTerms t = peak_terms(fast_exp(-0.5 * u * u), u, inv_sig);
            G[i] = t.G;
            H[i] = t.H;
            dG_dmu[i] = t.dG_dmu;
            dH_dmu[i] = t.dH_dmu;
            dG_dsig[i] = t.dG_dsig;
            dH_dsig[i] = t.dH_dsig;
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
     * sigma  ~ right-side half-width of the peak, see estimate_peak
     */
    static std::array<double, npar> estimate_par(NDView<double, 1> x,
                                                 NDView<double, 1> y) {
        const PeakEstimate peak = estimate_peak(x, y);
        const double S = peak.left_mean;
        // At x = mu, the erfc step contributes roughly S/2.
        const double A = peak.y_max - 0.5 * S;
        return {A, S, peak.mu, peak.sigma};
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

        const double inv_sig = 1.0 / sig;
        const double u = (x - mu) * inv_sig;
        const PeakTerms t = peak_terms(std::exp(-0.5 * u * u), u, inv_sig);
        return p0 - p1 * x + N * (t.G + C * t.H);
    }

    static void eval_and_grad(double x, const std::vector<double> &par,
                              double &f, std::array<double, npar> &g) {
        const double p0 = par[0];
        const double p1 = par[1];
        const double mu = par[2];
        const double sig = par[3];
        const double N = par[4];
        const double C = par[5];

        const double inv_sig = 1.0 / sig;
        const double u = (x - mu) * inv_sig;
        const PeakTerms t = peak_terms(std::exp(-0.5 * u * u), u, inv_sig);

        f = p0 - p1 * x + N * (t.G + C * t.H);

        g[0] = 1.0;
        g[1] = -x;
        g[2] = N * (t.dG_dmu + C * t.dH_dmu);
        g[3] = N * (t.dG_dsig + C * t.dH_dsig);
        g[4] = t.G + C * t.H;
        g[5] = N * t.H;
    }

    /**
     * @brief p0, p1 and N are linear: f = p0 * 1 + p1 * (-x) + N * (G + C H).
     * C multiplies N, so it stays a nonlinear parameter.
     */
    static constexpr std::array<std::size_t, 3> linear_par = {0, 1, 4};

    static void basis_columns(const double *__restrict x, ssize_t n,
                              const std::vector<double> &par,
                              double *__restrict phi, double *__restrict dphi) {
        const double mu = par[2];
        const double inv_sig = 1.0 / par[3];
        const double C = par[5];
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
            const double u = (xi - mu) * inv_sig;
            const PeakTerms t = peak_terms(fast_exp(-0.5 * u * u), u, inv_sig);
            one[i] = 1.0;
            minus_x[i] = -xi;
            shape[i] = t.G + C * t.H;
            d_mu[i] = t.dG_dmu + C * t.dH_dmu;
            d_sig[i] = t.dG_dsig + C * t.dH_dsig;
            d_C[i] = t.H;
        }
    }

    static bool is_valid(const std::vector<double> &par) {
        return par[3] > 0.0;
    }

    static std::array<double, npar> estimate_par(NDView<double, 1> x,
                                                 NDView<double, 1> y) {
        const PeakEstimate peak = estimate_peak(x, y);
        const double p0 = peak.right_mean;
        const double p1 = 0.0;
        const double N = std::max(peak.y_max - peak.right_mean, 1e-9);
        // Left plateau excess is roughly N*C.
        const double C = (peak.left_mean - peak.right_mean) / N;
        return {p0, p1, peak.mu, peak.sigma, N, C};
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

        const double inv_sig = 1.0 / sig;
        const double u = (x - mu) * inv_sig;
        const double ub = (x - r * mu) * inv_sig;
        const double G = std::exp(-0.5 * u * u);
        const double Gb = std::exp(-0.5 * ub * ub);
        const PeakTerms a = peak_terms(G, u, inv_sig);
        const PeakTerms b = peak_terms(Gb, ub, inv_sig);

        const double ka = a.G + C * a.H;
        const double kb = b.G + C * b.H;

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

        const double inv_sig = 1.0 / sig;
        const double u = (x - mu) * inv_sig;
        const double ub = (x - r * mu) * inv_sig;
        const double G = std::exp(-0.5 * u * u);
        const double Gb = std::exp(-0.5 * ub * ub);
        const PeakTerms a = peak_terms(G, u, inv_sig);
        const PeakTerms b = peak_terms(Gb, ub, inv_sig);

        const double ka = a.G + C * a.H;
        const double kb = b.G + C * b.H;

        f = p0 - p1 * x + N * (ka + q * kb);

        const double dka_dmu = a.dG_dmu + C * a.dH_dmu;
        const double dka_dsig = a.dG_dsig + C * a.dH_dsig;

        const double dkb_dmu_b = b.dG_dmu + C * b.dH_dmu;
        const double dkb_dsig = b.dG_dsig + C * b.dH_dsig;

        g[0] = 1.0;
        g[1] = -x;

        // mu_b = r * mu, so dmu_b/dmu = r
        g[2] = N * (dka_dmu + q * r * dkb_dmu_b);

        g[3] = N * (dka_dsig + q * dkb_dsig);

        g[4] = ka + q * kb;

        g[5] = N * (a.H + q * b.H);

        // mu_b = r * mu, so dmu_b/dr = mu
        g[6] = N * q * mu * dkb_dmu_b;

        g[7] = N * kb;
    }

    /**
     * @brief p0, p1 and N are linear: f = p0 * 1 + p1 * (-x) + N * (ka + q kb).
     * C, r and q multiply N or enter the peak shapes, so they stay nonlinear.
     */
    static constexpr std::array<std::size_t, 3> linear_par = {0, 1, 4};

    static void basis_columns(const double *__restrict x, ssize_t n,
                              const std::vector<double> &par,
                              double *__restrict phi, double *__restrict dphi) {
        const double mu = par[2];
        const double inv_sig = 1.0 / par[3];
        const double C = par[5];
        const double r = par[6];
        const double q = par[7];
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
            const double u = (xi - mu) * inv_sig;
            const double ub = (xi - mu_b) * inv_sig;
            const PeakTerms a = peak_terms(fast_exp(-0.5 * u * u), u, inv_sig);
            const PeakTerms b =
                peak_terms(fast_exp(-0.5 * ub * ub), ub, inv_sig);
            const double ka = a.G + C * a.H;
            const double kb = b.G + C * b.H;
            const double dka_dmu = a.dG_dmu + C * a.dH_dmu;
            const double dka_dsig = a.dG_dsig + C * a.dH_dsig;
            const double dkb_dmu_b = b.dG_dmu + C * b.dH_dmu;
            const double dkb_dsig = b.dG_dsig + C * b.dH_dsig;
            one[i] = 1.0;
            minus_x[i] = -xi;
            shape[i] = ka + q * kb;
            d_mu[i] = dka_dmu + q * r * dkb_dmu_b;
            d_sig[i] = dka_dsig + q * dkb_dsig;
            d_C[i] = a.H + q * b.H;
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
// RisingScurve and FallingScurve
// _____________________________________________________________________

/**
 * @brief S-curve (error-function step) with linear baseline and post-step
 *        slope. Direction = +1 rises with x, Direction = -1 falls.
 *
 * f(x) = (p0 + p1*x)
 *      + 0.5*(1 ± erf((x - mu) / (sqrt(2)*sigma))) * (A + C*(x - mu))
 *
 * Parameters:
 *   par[0] = p0  (baseline offset)
 *   par[1] = p1  (baseline slope)
 *   par[2] = mu  (inflection point)
 *   par[3] = sigma  (transition width, must be > 0)
 *   par[4] = A  (step amplitude)
 *   par[5] = C  (post-step slope)
 *
 * With the step S(x) = 0.5*(1 ± erf(z)), z = (x - mu) / (sqrt(2)*sigma):
 *   dS/dmu    = -(±1/sqrt(2*pi)) * exp(-z^2) / sigma
 *   dS/dsigma = -(±1/sqrt(2*pi)) * exp(-z^2) * (x - mu) / sigma^2
 */
template <int Direction> struct Scurve {
    static_assert(Direction == 1 || Direction == -1,
                  "an S-curve either rises (+1) or falls (-1)");

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
        const double step = 0.5 * (1.0 + sign * fast_erf(dx * inv_sqrt2 / p3));
        return (p0 + p1 * x) + step * (p4 + p5 * dx);
    }

    /** @brief Evaluate function value and partial derivatives in a single
     * pass. */
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
        const double step = 0.5 * (1.0 + sign * fast_erf_from_exp(z, e));
        const double amp = p4 + p5 * dx;

        f = (p0 + p1 * x) + step * amp;

        const double dSdp2 = dstep * e / p3;
        const double dSdp3 = dstep * e * dx / (p3 * p3);

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

    static void basis_columns(const double *__restrict x, ssize_t n,
                              const std::vector<double> &par,
                              double *__restrict phi, double *__restrict dphi) {
        const double p2 = par[2];
        const double inv_p3 = 1.0 / par[3];
        const double cz = inv_sqrt2 * inv_p3;
        const double ce = dstep * inv_p3;
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
            const double step = 0.5 * (1.0 + sign * fast_erf_from_exp(z, e));
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

    /**
     * @brief Data-driven initial parameter estimates.
     *
     * The baseline is a line through the ~10% of points before the step
     * turns on (the first points of a rising curve, the last of a falling
     * one), the plateau the mean of the ~10% at the other end.
     */
    static std::array<double, npar> estimate_par(NDView<double, 1> x,
                                                 NDView<double, 1> y) {
        constexpr bool rising = Direction > 0;
        const ssize_t n = y.size();
        const ssize_t n_tail = std::max<ssize_t>(n / 10, 2);
        const ssize_t base_begin = rising ? 0 : n - n_tail;
        const ssize_t plateau_begin = rising ? n - n_tail : 0;

        double sum_y = 0, sum_xy = 0, sum_x = 0, sum_x2 = 0;
        for (ssize_t i = base_begin; i < base_begin + n_tail; ++i) {
            sum_y += y[i];
            sum_x += x[i];
            sum_xy += x[i] * y[i];
            sum_x2 += x[i] * x[i];
        }
        double denom = n_tail * sum_x2 - sum_x * sum_x;
        double p1 = (std::abs(denom) > 1e-30)
                        ? (n_tail * sum_xy - sum_x * sum_y) / denom
                        : 0.0;
        double p0 = (sum_y - p1 * sum_x) / n_tail;

        double plateau = 0;
        for (ssize_t i = plateau_begin; i < plateau_begin + n_tail; ++i)
            plateau += y[i];
        plateau /= n_tail;

        // amplitude: plateau minus baseline at midpoint
        double x_mid = 0.5 * (x[0] + x[n - 1]);
        double baseline_at_mid = p0 + p1 * x_mid;
        double p4 = plateau - baseline_at_mid;

        // x of the first point past the given fraction of the step
        const auto crossing = [&](double fraction, double fallback) {
            const double level = baseline_at_mid + fraction * p4;
            for (ssize_t i = 0; i < n; ++i) {
                if (rising ? y[i] >= level : y[i] <= level)
                    return x[i];
            }
            return fallback;
        };

        // threshold: halfway between baseline and plateau
        double p2 = crossing(0.5, x_mid);

        // sigma: from the transition width between 10% and 90% of the step;
        // for a Gaussian CDF that width is 2 * 1.2816 * sigma
        double x_enter = crossing(rising ? 0.1 : 0.9, x[0]);
        double x_leave = crossing(rising ? 0.9 : 0.1, x[n - 1]);
        double p3 = std::max((x_leave - x_enter) / 2.5631, 1.0);

        double p5 = 0.0; // assume flat gain, let optimizer find the slope

        return {p0, p1, p2, p3, p4, p5};
    }

  private:
    static constexpr double sign = Direction;
    // dS/dmu = dstep * exp(-z^2) / sigma
    static constexpr double dstep = -sign * inv_sqrt_2pi;
};

/** @brief S-curve that rises with x, see Scurve. */
struct RisingScurve : Scurve<+1> {};

/** @brief S-curve that falls with x, see Scurve. */
struct FallingScurve : Scurve<-1> {};

} // namespace aare::model

/**
 * @brief The fit models: expands to X(Model) for every model in aare::model.
 *
 * The explicit instantiations of FitModel and the fit functions and the
 * Python bindings are generated from this list, so a new model is added
 * here once. It also needs its Minuit step scales in src/fit/MinuitSteps.hpp.
 */
#define AARE_FOR_EACH_FIT_MODEL(X)                                             \
    X(Pol1)                                                                    \
    X(Pol2)                                                                    \
    X(Gaussian)                                                                \
    X(GaussianErfcPlateau)                                                     \
    X(GaussianChargeSharing)                                                   \
    X(GaussianChargeSharingKb)                                                 \
    X(RisingScurve)                                                            \
    X(FallingScurve)
