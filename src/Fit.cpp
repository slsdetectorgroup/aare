#include "aare/Fit.hpp"
#include <lmcurve2.h>
#include <lmfit.hpp>

namespace aare {

namespace func {

double gauss(const double x, const double *par) {
    return par[0] * exp(-pow(x - par[1], 2) / (2 * pow(par[2], 2)));
}

NDArray<double, 1> gauss(NDView<double, 1> x, NDView<double, 1> par) {
    NDArray<double, 1> y({x.size()}, 0);
    for (size_t i = 0; i < x.size(); i++) {
        y(i) = gauss(x(i), par.data());
    }
    return y;
}

double affine(const double x, const double *par) { return par[0] * x + par[1]; }

NDArray<double, 1> affine(NDView<double, 1> x, NDView<double, 1> par) {
    NDArray<double, 1> y({x.size()}, 0);
    for (size_t i = 0; i < x.size(); i++) {
        y(i) = affine(x(i), par.data());
    }
    return y;
}

} // namespace func

NDArray<double, 3> fit_gaus(NDView<double, 3> data, NDView<double, 1> x) {

    NDArray<double, 3> result({data.shape(0), data.shape(1), 3}, 0);

    for (ssize_t row = 0; row < data.shape(0); row++) {
        for (ssize_t col = 0; col < data.shape(1); col++) {
            NDView<double, 1> y(&data(row, col, 0), {data.shape(2)});
            auto res = fit_gaus(y, x);
            result(row, col, 0) = res(0);
            result(row, col, 1) = res(1);
            result(row, col, 2) = res(2);
        }
    }
    return result;
}
NDArray<double, 1> fit_gaus(NDView<double, 1> data, NDView<double, 1> x) {
    NDArray<double, 1> result({3}, 0);
    lm_control_struct control = lm_control_double;

    // Estimate the initial parameters for the fit
    std::vector<double> start_par{0, 0, 0};
    auto e = std::max_element(data.begin(), data.end());
    auto idx = std::distance(data.begin(), e);

    start_par[0] = *e; // For amplitude we use the maximum value
    start_par[1] =
        x[idx]; // For the mean we use the x value of the maximum value

    // For sigma we estimate the fwhm and divide by 2.35
    // assuming equally spaced x values
    auto delta = x[1] - x[0];
    start_par[2] =
        std::count_if(data.begin(), data.end(),
                      [e, delta](double val) { return val > *e / 2; }) *
        delta / 2.35;

    // fmt::print("start_par: {} {} {}\n", start_par[0], start_par[1],
    // start_par[2]);
    lmfit::result_t res(start_par);
    lmcurve(res.par.size(), res.par.data(), x.size(), x.data(), data.data(),
            aare::func::gauss, &control, &res.status);

    result(0) = res.par[0];
    result(1) = res.par[1];
    result(2) = res.par[2];

    return result;
}

NDArray<double, 3> fit_gaus(NDView<double, 3> data, NDView<double, 1> x,
                            NDView<double, 3> data_err) {
    NDArray<double, 3> result({data.shape(0), data.shape(1), 6}, 0);

    for (ssize_t row = 0; row < data.shape(0); row++) {
        for (ssize_t col = 0; col < data.shape(1); col++) {
            NDView<double, 1> y(&data(row, col, 0), {data.shape(2)});
            NDView<double, 1> y_err(&data_err(row, col, 0), {data.shape(2)});
            auto res = fit_gaus(y, x, y_err);
            result(row, col, 0) = res[0];
            result(row, col, 1) = res[1];
            result(row, col, 2) = res[2];
            result(row, col, 3) = res[3];
            result(row, col, 4) = res[4];
            result(row, col, 5) = res[5];
        }
    }
    return result;
}

void fit_gaus(NDView<double, 1> x, NDView<double, 3> y, NDView<double, 3> y_err,
              NDView<double, 3> par_out, NDView<double, 3> par_err_out) {
    for (ssize_t row = 0; row < y.shape(0); row++) {
        for (ssize_t col = 0; col < y.shape(1); col++) {
            NDView<double, 1> y_view(&y(row, col, 0), {y.shape(2)});
            NDView<double, 1> y_err_view(&y_err(row, col, 0), {y_err.shape(2)});
            NDView<double, 1> par_out_view(&par_out(row, col, 0),
                                           {par_out.shape(2)});
            NDView<double, 1> par_err_out_view(&par_err_out(row, col, 0),
                                               {par_err_out.shape(2)});
            fit_gaus(x, y_view, y_err_view, par_out_view, par_err_out_view);
        }
    }
}

void fit_gaus(NDView<double, 1> x, NDView<double, 1> y, NDView<double, 1> y_err,
              NDView<double, 1> par_out, NDView<double, 1> par_err_out) {
    // Check that we have the correct sizes
    if (y.size() != x.size() || y.size() != y_err.size() ||
        par_out.size() != 3 || par_err_out.size() != 3) {
        throw std::runtime_error("Data, x, data_err must have the same size "
                                 "and par_out, par_err_out must have size 3");
    }

    lm_control_struct control = lm_control_double;

    // Estimate the initial parameters for the fit
    std::vector<double> start_par{0, 0, 0};
    std::vector<double> start_par_err{0, 0, 0};
    std::vector<double> start_cov{0, 0, 0, 0, 0, 0, 0, 0, 0};

    auto e = std::max_element(y.begin(), y.end());
    auto idx = std::distance(y.begin(), e);
    start_par[0] = *e; // For amplitude we use the maximum value
    start_par[1] =
        x[idx]; // For the mean we use the x value of the maximum value

    // For sigma we estimate the fwhm and divide by 2.35
    // assuming equally spaced x values
    auto delta = x[1] - x[0];
    start_par[2] =
        std::count_if(y.begin(), y.end(),
                      [e, delta](double val) { return val > *e / 2; }) *
        delta / 2.35;

    lmfit::result_t res(start_par);
    lmfit::result_t res_err(start_par_err);
    lmfit::result_t cov(start_cov);

    // TODO can we make lmcurve write the result directly where is should be?
    lmcurve2(res.par.size(), res.par.data(), res_err.par.data(), cov.par.data(),
             x.size(), x.data(), y.data(), y_err.data(), aare::func::gauss,
             &control, &res.status);

    par_out(0) = res.par[0];
    par_out(1) = res.par[1];
    par_out(2) = res.par[2];
    par_err_out(0) = res_err.par[0];
    par_err_out(1) = res_err.par[1];
    par_err_out(2) = res_err.par[2];
}

NDArray<double, 1> fit_gaus(NDView<double, 1> data, NDView<double, 1> x,
                            NDView<double, 1> data_err) {

    NDArray<double, 1> result({6}, 0);
    NDView<double, 1> y(&data(0), {data.size()});
    NDView<double, 1> y_err(&data_err(0), {data_err.size()});
    NDView<double, 1> par_out(&result(0), {3});
    NDView<double, 1> par_err_out(&result(3), {3});
    fit_gaus(x, y, y_err, par_out, par_err_out);

    return result;
}

NDArray<double, 3> fit_affine(NDView<double, 3> data, NDView<double, 1> x,
                              NDView<double, 3> data_err) {
    NDArray<double, 3> result({data.shape(0), data.shape(1), 4}, 0);
    for (ssize_t row = 0; row < data.shape(0); row++) {
        for (ssize_t col = 0; col < data.shape(1); col++) {
            NDView<double, 1> y(&data(row, col, 0), {data.shape(2)});
            NDView<double, 1> y_err(&data_err(row, col, 0), {data.shape(2)});
            auto res = fit_affine(y, x, y_err);
            result(row, col, 0) = res[0];
            result(row, col, 1) = res[1];
            result(row, col, 2) = res[2];
            result(row, col, 3) = res[3];
        }
    }
    return result;
}

void fit_affine(NDView<double, 1> x, NDView<double, 1> y,
                NDView<double, 1> y_err, NDView<double, 1> par_out,
                NDView<double, 1> par_err_out) {
    // Check that we have the correct sizes
    if (y.size() != x.size() || y.size() != y_err.size() ||
        par_out.size() != 2 || par_err_out.size() != 2) {
        throw std::runtime_error("Data, x, data_err must have the same size "
                                 "and par_out, par_err_out must have size 2");
    }

    lm_control_struct control = lm_control_double;

    // Estimate the initial parameters for the fit
    std::vector<double> start_par{0, 0};
    std::vector<double> start_par_err{0, 0};
    std::vector<double> start_cov{0, 0, 0, 0};

    auto y2 = std::max_element(y.begin(), y.end());
    auto x2 = x[std::distance(y.begin(), y2)];
    auto y1 = std::min_element(y.begin(), y.end());
    auto x1 = x[std::distance(y.begin(), y1)];

    start_par[0] =
        (*y2 - *y1) / (x2 - x1); // For amplitude we use the maximum value
    start_par[1] =
        *y1 - ((*y2 - *y1) / (x2 - x1)) *
                  x1; // For the mean we use the x value of the maximum value

    lmfit::result_t res(start_par);
    lmfit::result_t res_err(start_par_err);
    lmfit::result_t cov(start_cov);

    lmcurve2(res.par.size(), res.par.data(), res_err.par.data(), cov.par.data(),
             x.size(), x.data(), y.data(), y_err.data(), aare::func::affine,
             &control, &res.status);

    par_out(0) = res.par[0];
    par_out(1) = res.par[1];
    par_err_out(0) = res_err.par[0];
    par_err_out(1) = res_err.par[1];
}

void fit_affine(NDView<double, 1> x, NDView<double, 3> y,
                NDView<double, 3> y_err, NDView<double, 3> par_out,
                NDView<double, 3> par_err_out) {
    for (ssize_t row = 0; row < y.shape(0); row++) {
        for (ssize_t col = 0; col < y.shape(1); col++) {
            NDView<double, 1> y_view(&y(row, col, 0), {y.shape(2)});
            NDView<double, 1> y_err_view(&y_err(row, col, 0), {y_err.shape(2)});
            NDView<double, 1> par_out_view(&par_out(row, col, 0),
                                           {par_out.shape(2)});
            NDView<double, 1> par_err_out_view(&par_err_out(row, col, 0),
                                               {par_err_out.shape(2)});
            fit_affine(x, y_view, y_err_view, par_out_view, par_err_out_view);
        }
    }
}

NDArray<double, 1> fit_affine(NDView<double, 1> data, NDView<double, 1> x,
                              NDView<double, 1> data_err) {
    NDArray<double, 1> result({4}, 0);
    NDView<double, 1> par(&result(0), {2});
    NDView<double, 1> par_err(&result(2), {2});
    fit_affine(x, data, data_err, par, par_err);
    return result;
}

} // namespace aare