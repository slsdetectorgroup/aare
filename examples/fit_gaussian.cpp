// SPDX-License-Identifier: MPL-2.0
// Fit a Gaussian peak to a one-dimensional histogram.
#include "aare/Fit.hpp"
#include "aare/NDArray.hpp"

#include <fmt/core.h>

#include <cmath>
#include <cstdlib>
#include <random>

int main() {
    // Histogram 10000 normally distributed values (mean 25, sigma 3) into
    // 60 bins covering [10, 40)
    constexpr ssize_t n_bins = 60;
    constexpr double lower_edge = 10.0;
    constexpr double bin_width = 0.5;

    aare::NDArray<double, 1> x({n_bins});
    aare::NDArray<double, 1> y({n_bins}, 0.0);
    aare::NDArray<double, 1> y_err({n_bins});

    std::mt19937 generator(42);
    std::normal_distribution<double> distribution(25.0, 3.0);
    for (int i = 0; i < 10000; ++i) {
        const double position =
            (distribution(generator) - lower_edge) / bin_width;
        if (position >= 0.0 && position < static_cast<double>(n_bins))
            y(static_cast<ssize_t>(position)) += 1.0;
    }

    // x is the center of each bin and the uncertainty of a bin is the square
    // root of its content. Empty bins get y_err == 0 and are left out of the
    // fit.
    for (ssize_t i = 0; i < n_bins; ++i) {
        x(i) = lower_edge + (static_cast<double>(i) + 0.5) * bin_width;
        y_err(i) = std::sqrt(y(i));
    }

    // Create the model and fit it. Start values are estimated from the data.
    aare::FitModel<aare::model::Gaussian> model;
    model.SetComputeErrors(true);

    // With errors enabled the result holds
    // [A, mu, sigma, A_err, mu_err, sigma_err, chi2]
    const auto result =
        aare::fit_pixel(model, x.view(), y.view(), y_err.view());

    constexpr ssize_t n_par = aare::model::Gaussian::npar;
    for (ssize_t i = 0; i < n_par; ++i) {
        fmt::print("{:>5} = {:7.2f} +- {:.2f}\n",
                   model.GetParName(static_cast<unsigned int>(i)), result(i),
                   result(n_par + i));
    }
    fmt::print(" chi2 = {:7.2f}\n", result(2 * n_par));

    // The examples run as tests, so report a fit that missed the peak
    const double mu = result(1);
    const double sigma = result(2);
    const bool found_peak =
        std::abs(mu - 25.0) < 0.2 && std::abs(sigma - 3.0) < 0.2;
    return found_peak ? EXIT_SUCCESS : EXIT_FAILURE;
}
