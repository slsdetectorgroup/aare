// SPDX-License-Identifier: MPL-2.0
#pragma once

#include <array>
#include <cmath>
#include <vector>

namespace aare::model {

inline constexpr double inv_sqrt2    = 0.70710678118654752440;
inline constexpr double inv_sqrt_2pi = 0.39894228040143267794;

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

        g[0] = 1.0;
        g[1] = x;
        g[2] = dSdp2 * amp - step * p5;
        g[3] = dSdp3 * amp;
        g[4] = step;
        g[5] = step * dx;
    }
};

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
        const double dSdp2 = +inv_sqrt_2pi * e / p3;
        const double dSdp3 = +inv_sqrt_2pi * e * dx / (p3 * p3);

        g[0] = 1.0;
        g[1] = x;
        g[2] = dSdp2 * amp - step * p5;
        g[3] = dSdp3 * amp;
        g[4] = step;
        g[5] = step * dx;
    }
};

} // namespace aare::model