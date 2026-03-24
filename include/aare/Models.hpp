// SPDX-License-Identifier: MPL-2.0
#pragma once

#include <array>
#include <cmath>
#include <vector>
#include <limits>

namespace aare::model {

inline constexpr double inv_sqrt2    = 0.70710678118654752440;
inline constexpr double inv_sqrt_2pi = 0.39894228040143267794;


/**
 * @brief Per-parameter metadata: name and optional default bounds.
 *
 * Used by FitModel to generically initialise MnUserParameters.
 * Unbounded directions use ±no_bound as sentinels.
 */
struct ParamInfo {
    const char* name;
    double default_lo;
    double default_hi;
};
 
inline constexpr double no_bound = std::numeric_limits<double>::infinity();

/**
 * @brief Compute data-range statistics used by step-size estimators.
 *
 * Model-independent, called once per pixel.
 */
inline void compute_ranges(NDView<double, 1> x,
                           NDView<double, 1> y,
                           double& x_range,
                           double& y_range,
                           double& slope_scale)
{
    const double x_min = std::min(x[0], x[x.size() - 1]);
    const double x_max = std::max(x[0], x[x.size() - 1]);
    x_range = std::max(x_max - x_min, 1e-12);
 
    double y_min = y[0], y_max = y[0];
    for (ssize_t i = 1; i < y.size(); ++i) {
        y_min = std::min(y_min, y[i]);
        y_max = std::max(y_max, y[i]);
    }
    y_range     = std::max(y_max - y_min, 1e-9);
    slope_scale = std::max(y_range / x_range, 1e-9);
}

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
        {"A",   -no_bound, no_bound},
        {"mu",  -no_bound, no_bound},
        {"sig",  1e-12,    no_bound},
    }};

    static double eval(double x, const std::vector<double>& par) {
        const double A   = par[0];
        const double mu  = par[1];
        const double sig = par[2];

        const double dx        = x - mu;
        const double inv_2sig2 = 1.0 / (2.0 * sig * sig);
        return A * std::exp(-dx * dx * inv_2sig2);
    }

    static void eval_and_grad(double x,
                              const std::vector<double>& par,
                              double& f,
                              std::array<double, npar>& g)
    {
        const double A   = par[0];
        const double mu  = par[1];
        const double sig = par[2];

        const double dx        = x - mu;
        const double inv_2sig2 = 1.0 / (2.0 * sig * sig);
        const double e         = std::exp(-dx * dx * inv_2sig2);

        f = A * e;

        g[0] = e;                                          // df/dA
        g[1] = 2.0 * A * e * dx * inv_2sig2;              // df/dmu    = A*e*(x-mu)/sig^2
        g[2] = 2.0 * A * e * dx * dx * inv_2sig2 / sig;   // df/dsigma = A*e*(x-mu)^2/sig^3
    }

    /** @brief Reject degenerate sigma (zero width). */
    static bool is_valid(const std::vector<double>& par) {
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
                                                  NDView<double, 1> y)
    {
        // find peak
        ssize_t i_max = 0;
        for (ssize_t i = 1; i < y.size(); ++i)
            if (y[i] > y[i_max]) i_max = i;
 
        const double A  = y[i_max];
        const double mu = x[i_max];
 
        // FWHM estimate
        const double half = A * 0.5;
        double x_lo = mu, x_hi = mu;
        for (ssize_t i = i_max; i >= 0; --i)
            if (y[i] < half) { x_lo = x[i]; break; }
        for (ssize_t i = i_max; i < y.size(); ++i)
            if (y[i] < half) { x_hi = x[i]; break; }
        const double sig = std::max((x_hi - x_lo) / 2.35, 1e-6);
 
        return {A, mu, sig};
    }

    /**
     * @brief Data-driven Minuit step sizes.
     */
    static void compute_steps(const std::array<double, npar>& start,
                              double x_range, double y_range, double /*slope_scale*/,
                              std::array<double, npar>& steps)
    {
        steps[0] = std::max(0.1 * std::abs(start[0]), 0.1 * y_range);
        steps[1] = 0.05 * x_range;
        steps[2] = 0.05 * x_range;
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
 * f(x) = (p0 + p1*x) + 0.5*(1 + erf((x - p2) / (sqrt(2)*p3))) * (p4 + p5*(x - p2))
 *
 * Parameters:
 *   par[0] = p0  (baseline offset)
 *   par[1] = p1  (baseline slope)
 *   par[2] = p2  (threshold / inflection point)
 *   par[3] = p3  (transition width, must be > 0)
 *   par[4] = p4  (step amplitude)
 *   par[5] = p5  (post-step slope)
 */
struct RisingScurve {
    static constexpr std::size_t npar = 6;

    static constexpr std::array<ParamInfo, npar> param_info = {{
        {"p0", -no_bound, no_bound},
        {"p1", -no_bound, no_bound},
        {"p2", -no_bound, no_bound},
        {"p3",  1e-12,    no_bound},
        {"p4", -no_bound, no_bound},
        {"p5", -no_bound, no_bound},
    }};

    static double eval(double x, const std::vector<double>& par) {
        const double p0 = par[0];
        const double p1 = par[1];
        const double p2 = par[2];
        const double p3 = par[3];
        const double p4 = par[4];
        const double p5 = par[5];

        const double dx   = x - p2;
        const double step = 0.5 * (1.0 + std::erf(dx * inv_sqrt2 / p3));
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
    static void eval_and_grad(double x,
                              const std::vector<double>& par,
                              double& f,
                              std::array<double, npar>& g)
    {
        const double p0 = par[0];
        const double p1 = par[1];
        const double p2 = par[2];
        const double p3 = par[3];
        const double p4 = par[4];
        const double p5 = par[5];

        const double dx   = x - p2;
        const double z    = dx * inv_sqrt2 / p3;
        const double step = 0.5 * (1.0 + std::erf(z));
        const double amp  = p4 + p5 * dx;

        f = (p0 + p1 * x) + step * amp;

        const double e     = std::exp(-z * z);
        const double dSdp2 = -inv_sqrt_2pi * e / p3;
        const double dSdp3 = -inv_sqrt_2pi * e * dx / (p3 * p3);

        g[0] = 1.0;                         // df/dp0
        g[1] = x;                            // df/dp1
        g[2] = dSdp2 * amp - step * p5;     // df/dp2
        g[3] = dSdp3 * amp;                 // df/dp3
        g[4] = step;                         // df/dp4
        g[5] = step * dx;                   // df/dp5
    }

    /** @brief Reject degenerate width (zero transition width). */
    static bool is_valid(const std::vector<double>& par) {
        return par[3] != 0.0;
    }

        
    /** @brief Data-driven initial parameter estimates for a rising S-curve. */
    static std::array<double, npar> estimate_par(NDView<double, 1> x,
                                                  NDView<double, 1> y)
    {
        const ssize_t n = y.size();

        // baseline: average of first ~10% of points (before turn-on)
        ssize_t n_base = std::max<ssize_t>(n / 10, 2);
        double sum_y = 0, sum_xy = 0, sum_x = 0, sum_x2 = 0;
        for (ssize_t i = 0; i < n_base; ++i) {
            sum_y  += y[i];
            sum_x  += x[i];
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
            if (y[i] >= y_10) { x_10 = x[i]; break; }
        }
        for (ssize_t i = 0; i < n; ++i) {
            if (y[i] >= y_90) { x_90 = x[i]; break; }
        }
        // for a Gaussian CDF: 10%-90% width = 2 * 1.2816 * sigma
        double p3 = std::max((x_90 - x_10) / 2.5631, 1.0);

        double p5 = 0.0; // assume flat gain, let optimizer find the slope

        return {p0, p1, p2, p3, p4, p5};
    }
 
    static void compute_steps(const std::array<double, npar>& start,
                              double x_range, double y_range, double slope_scale,
                              std::array<double, npar>& steps)
    {
        steps[0] = std::max(0.1 * std::abs(start[0]), 0.1 * y_range);
        steps[1] = 0.1 * slope_scale;
        steps[2] = 0.05 * x_range;
        steps[3] = 0.05 * x_range;
        steps[4] = std::max(0.1 * std::abs(start[4]), 0.1 * y_range);
        steps[5] = 0.1 * slope_scale;
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
 * f(x) = (p0 + p1*x) + 0.5*(1 - erf((x - p2) / (sqrt(2)*p3))) * (p4 + p5*(x - p2))
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
        {"p2", -no_bound, no_bound},
        {"p3",  1e-12,    no_bound},
        {"p4", -no_bound, no_bound},
        {"p5", -no_bound, no_bound},
    }};

    static double eval(double x, const std::vector<double>& par) {
        const double p0 = par[0];
        const double p1 = par[1];
        const double p2 = par[2];
        const double p3 = par[3];
        const double p4 = par[4];
        const double p5 = par[5];

        const double dx   = x - p2;
        const double step = 0.5 * (1.0 - std::erf(dx * inv_sqrt2 / p3));
        return (p0 + p1 * x) + step * (p4 + p5 * dx);
    }

    static void eval_and_grad(double x,
                              const std::vector<double>& par,
                              double& f,
                              std::array<double, npar>& g)
    {
        const double p0 = par[0];
        const double p1 = par[1];
        const double p2 = par[2];
        const double p3 = par[3];
        const double p4 = par[4];
        const double p5 = par[5];

        const double dx   = x - p2;
        const double z    = dx * inv_sqrt2 / p3;
        const double step = 0.5 * (1.0 - std::erf(z));
        const double amp  = p4 + p5 * dx;

        f = (p0 + p1 * x) + step * amp;

        const double e     = std::exp(-z * z);
        const double dSdp2 = +inv_sqrt_2pi * e / p3;          // sign flipped vs rising
        const double dSdp3 = +inv_sqrt_2pi * e * dx / (p3 * p3);

        g[0] = 1.0;
        g[1] = x;
        g[2] = dSdp2 * amp - step * p5;
        g[3] = dSdp3 * amp;
        g[4] = step;
        g[5] = step * dx;
    }

    /** @brief Reject degenerate width (zero transition width). */
    static bool is_valid(const std::vector<double>& par) {
        return par[3] != 0.0;
    }

    /** @brief Data-driven initial parameter estimates for a falling S-curve. */
    static std::array<double, npar> estimate_par(NDView<double, 1> x,
                                                NDView<double, 1> y)
    {
        const ssize_t n = y.size();

        // baseline: last ~10% of points (after turn-off)
        ssize_t n_base = std::max<ssize_t>(n / 10, 2);
        double sum_y = 0, sum_xy = 0, sum_x = 0, sum_x2 = 0;
        for (ssize_t i = n - n_base; i < n; ++i) {
            sum_y  += y[i];
            sum_x  += x[i];
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
            if (y[i] <= y_90) { x_90 = x[i]; break; }
        }
        for (ssize_t i = 0; i < n; ++i) {
            if (y[i] <= y_10) { x_10 = x[i]; break; }
        }
        // same CDF relationship: 10%-90% width = 2 * 1.2816 * sigma
        double p3 = std::max((x_10 - x_90) / 2.5631, 1.0);

        double p5 = 0.0;

        return {p0, p1, p2, p3, p4, p5};
    }
 
    static void compute_steps(const std::array<double, npar>& start,
                              double x_range, double y_range, double slope_scale,
                              std::array<double, npar>& steps)
    {
        RisingScurve::compute_steps(start, x_range, y_range, slope_scale, steps);
    }
};

} // namespace aare::model
