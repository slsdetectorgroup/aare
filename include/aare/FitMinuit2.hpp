// SPDX-License-Identifier: MPL-2.0
#pragma once

#include "aare/FitModel.hpp"
#include "aare/NDArray.hpp"

namespace aare::minuit2 {

// ---------------------------------------------------------------------------
// Optional Minuit2 (Migrad + Hesse) fitting backend, kept for side-by-side
// validation of the built-in Levenberg-Marquardt solver.
//
// The definitions are only compiled into aare_core when CMake is configured
// with -DAARE_MINUIT2=ON (compile definition AARE_MINUIT2); without it these
// functions fail to link. Arguments, result layout and failure behaviour are
// the same as for aare::fit_pixel and aare::fit_3d, see aare/Fit.hpp.
// ---------------------------------------------------------------------------

template <typename Model>
NDArray<double, 1> fit_pixel(const FitModel<Model> &model, NDView<double, 1> x,
                             NDView<double, 1> y, NDView<double, 1> y_err);

template <typename Model>
NDArray<double, 1> fit_pixel(const FitModel<Model> &model, NDView<double, 1> x,
                             NDView<double, 1> y);

template <typename Model>
void fit_3d(const FitModel<Model> &model, NDView<double, 1> x,
            NDView<double, 3> y, NDView<double, 3> y_err,
            NDView<double, 3> par_out, NDView<double, 3> err_out,
            NDView<double, 2> chi2_out, int n_threads);

} // namespace aare::minuit2
