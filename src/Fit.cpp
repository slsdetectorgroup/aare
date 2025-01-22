#include "aare/Fit.hpp"
#include <lmfit.hpp>
#include <lmcurve2.h>

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

double affine(const double x, const double *par) {
    return par[0] * x + par[1];
}

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

    //Estimate the initial parameters for the fit 
    std::vector<double> start_par{0, 0, 0};
    auto e = std::max_element(data.begin(), data.end());
    auto idx = std::distance(data.begin(), e);
    
    start_par[0] = *e;      // For amplitude we use the maximum value
    start_par[1] = x[idx];  // For the mean we use the x value of the maximum value
    
    // For sigma we estimate the fwhm and divide by 2.35
    // assuming equally spaced x values
    auto delta = x[1] - x[0];
    start_par[2] = std::count_if(data.begin(), data.end(),
                                    [e, delta](double val) { return val > *e / 2; }) * delta / 2.35;

    // fmt::print("start_par: {} {} {}\n", start_par[0], start_par[1], start_par[2]);
    lmfit::result_t res(start_par);
    lmcurve(res.par.size(), res.par.data(), x.size(), x.data(),
            data.data(), aare::func::gauss, &control, &res.status);

    result(0) = res.par[0];
    result(1) = res.par[1];
    result(2) = res.par[2];

    return result;
}


NDArray<double, 3> fit_gaus(NDView<double, 3> data, NDView<double, 1> x, NDView<double, 3> data_err) {
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


NDArray<double, 1> fit_gaus(NDView<double, 1> data, NDView<double, 1> x, NDView<double, 1> data_err) {

    NDArray<double, 1> result({6}, 0);
    lm_control_struct control = lm_control_double;

    //Estimate the initial parameters for the fit 
    std::vector<double> start_par{0, 0, 0};
    std::vector<double> start_par_err{0, 0, 0};
    std::vector<double> start_cov{0, 0, 0, 0, 0, 0, 0, 0, 0};

    auto e = std::max_element(data.begin(), data.end());
    auto idx = std::distance(data.begin(), e);
    start_par[0] = *e;      // For amplitude we use the maximum value
    start_par[1] = x[idx];  // For the mean we use the x value of the maximum value
    
    // For sigma we estimate the fwhm and divide by 2.35
    // assuming equally spaced x values
    auto delta = x[1] - x[0];
    start_par[2] = std::count_if(data.begin(), data.end(),
                                    [e, delta](double val) { return val > *e / 2; }) * delta / 2.35;


    lmfit::result_t res(start_par);
    lmfit::result_t res_err(start_par_err);
    lmfit::result_t cov(start_cov);

    lmcurve2(res.par.size(), res.par.data(), res_err.par.data(), cov.par.data(), x.size(), x.data(),
            data.data(), data_err.data(), aare::func::gauss, &control, &res.status);


    result(0) = res.par[0];
    result(1) = res.par[1];
    result(2) = res.par[2];
    result(3) = res_err.par[0];
    result(4) = res_err.par[1];
    result(5) = res_err.par[2];

    return result;
}

NDArray<double, 3> fit_affine(NDView<double, 3> data, NDView<double, 1> x, NDView<double, 3> data_err) {
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

NDArray<double, 1> fit_affine(NDView<double, 1> data, NDView<double, 1> x, NDView<double, 1> data_err) {
    // std::vector<double> start_par{300, 3300, 50};

    NDArray<double, 1> result({4}, 0);
    // NDArray<double, 1> result_err({3}, 0);
    lm_control_struct control = lm_control_double;

    //Estimate the initial parameters for the fit 
    std::vector<double> start_par{0, 0};
    std::vector<double> start_par_err{0, 0};
    std::vector<double> start_cov{0, 0, 0, 0};


    auto y2 = std::max_element(data.begin(), data.end());
    auto x2 = x[std::distance(data.begin(), y2)];
    auto y1 = std::min_element(data.begin(), data.end());
    auto x1 = x[std::distance(data.begin(), y1)];

    // fmt::print("y2: {}\n", *y2);
    // fmt::print("x2: {}\n", x2);
    // fmt::print("y1: {}\n", *y1);
    // fmt::print("x1: {}\n", x1);
            
    start_par[0] = (*y2 - *y1) / (x2 - x1);      // For amplitude we use the maximum value
    start_par[1] = *y1 - ((*y2 - *y1) / (x2 - x1)) * x1;  // For the mean we use the x value of the maximum value
    

    lmfit::result_t res(start_par);
    lmfit::result_t res_err(start_par_err);
    lmfit::result_t cov(start_cov);

    lmcurve2(res.par.size(), res.par.data(), res_err.par.data(), cov.par.data(), x.size(), x.data(),
            data.data(), data_err.data(), aare::func::affine, &control, &res.status);

    result(0) = res.par[0];
    result(1) = res.par[1];
    result(2) = res_err.par[0];
    result(3) = res_err.par[1];

    return result;
}


} // namespace aare