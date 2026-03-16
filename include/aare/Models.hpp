// SPDX-License-Identifier: MPL-2.0
#pragma once

#include <array>
#include <cmath>
#include <vector>

namespace aare::model {

inline constexpr double inv_sqrt2    = 0.70710678118654752440;
inline constexpr double inv_sqrt_2pi = 0.39894228040143267794;

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
};

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
struct SCurveRising {
    static constexpr std::size_t npar = 6;

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
};

/**
 * @brief Falling S-curve (complementary error-function step) with linear
 *        baseline and post-step slope.
 *
 * f(x) = (p0 + p1*x) + 0.5*(1 - erf((x - p2) / (sqrt(2)*p3))) * (p4 + p5*(x - p2))
 *
 * Parameters are identical to SCurveRising.  The only difference is the
 * sign of the erf term, which flips the step direction (and the signs of
 * dS/dp2 and dS/dp3 in the gradient).
 */
struct SCurveFalling {
    static constexpr std::size_t npar = 6;

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
};

} // namespace aare::model
