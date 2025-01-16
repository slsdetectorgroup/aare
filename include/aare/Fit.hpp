#pragma once

#include <cmath>
#include <fmt/core.h>
#include <vector>

#include "aare/NDArray.hpp"

namespace aare {

namespace func {
double gauss(const double x, const double *par);
} // namespace func

NDArray<double, 3> fit_gaus(NDView<double, 3> data, NDView<double, 1> x);
NDArray<double, 1> fit_gaus(NDView<double, 1> data, NDView<double, 1> x);

} // namespace aare