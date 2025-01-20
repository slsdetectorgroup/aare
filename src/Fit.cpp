#include "aare/Fit.hpp"
#include <lmfit.hpp>
#include <lmcurve2.h>

namespace aare {

namespace func {

double gauss(const double x, const double *par) {
    return par[0] * exp(-pow(x - par[1], 2) / (2 * pow(par[2], 2)));
}

double affine(const double x, const double *par) {
    return par[0] * x + par[1];
}

} // namespace func

NDArray<double, 3> fit_gaus(NDView<double, 3> data, NDView<double, 1> x) {
    // std::vector<double> start_par{300, 3300, 50};

    NDArray<double, 3> result({data.shape(0), data.shape(1), 3}, 0);

    lm_control_struct control = lm_control_double;

    // fmt::print("data.shape(0): {}\n", data.shape(0));
    // fmt::print("data.shape(1): {}\n", data.shape(1));
    // fmt::print("data.shape(2): {}\n", data.shape(2));

    auto ptr = data.data();

    for (ssize_t row = 0; row < data.shape(0); row++) {
        for (ssize_t col = 0; col < data.shape(1); col++) {

            //Estimate the initial parameters for the fit 
            std::vector<double> start_par{0, 0, 0};
            auto e = std::max_element(ptr, ptr + data.shape(2));
            auto idx = std::distance(ptr, e);
            
            start_par[0] = *e;      // For amplitude we use the maximum value
            start_par[1] = x[idx];  // For the mean we use the x value of the maximum value
            
            // For sigma we estimate the fwhm and divide by 2.35
            // assuming equally spaced x values
            auto delta = x[1] - x[0];
            start_par[2] = std::count_if(ptr, ptr + data.shape(2),
                                         [e, delta](double val) { return val > *e / 2; }) * delta / 2.35;

            // if (row == 0 && col == 0) {
            //     fmt::print("start_par: {} {} {}\n", start_par[0], start_par[1], start_par[2]);
            //     for (size_t i = 0; i < data.shape(2); i++) {
            //         fmt::print("data({},{},{}): {} {}\n", row, col, i, x[i], data(row, col, i));
            //     }
            // }

            lmfit::result_t res(start_par);
            lmcurve(res.par.size(), res.par.data(), x.size(), x.data(),
                    &data(row, col,0), aare::func::gauss, &control, &res.status);
            // fmt::print("par: {} {} {}\n", res.par[0], res.par[1], res.par[2]);

            result(row, col, 0) = res.par[0];
            result(row, col, 1) = res.par[1];
            result(row, col, 2) = res.par[2];

            ptr += data.shape(2);
        }

    }
    return result;

}
NDArray<double, 1> fit_gaus(NDView<double, 1> data, NDView<double, 1> x) {
    // std::vector<double> start_par{300, 3300, 50};

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


    lmfit::result_t res(start_par);
    lmcurve(res.par.size(), res.par.data(), x.size(), x.data(),
            data.data(), aare::func::gauss, &control, &res.status);

    result(0) = res.par[0];
    result(1) = res.par[1];
    result(2) = res.par[2];

    return result;
}
// ============================================================================================
// With errors
NDArray<double, 3> fit_gaus2(NDView<double, 3> data, NDView<double, 1> x, NDView<double, 3> data_err) {
    // std::vector<double> start_par{300, 3300, 50};

    NDArray<double, 3> result({data.shape(0), data.shape(1), 6}, 0);
    // NDArray<double, 3> result_error({data.shape(0), data.shape(1), 3}, 0);

    lm_control_struct control = lm_control_double;

    // fmt::print("data.shape(0): {}\n", data.shape(0));
    // fmt::print("data.shape(1): {}\n", data.shape(1));
    // fmt::print("data.shape(2): {}\n", data.shape(2));

    auto ptr = data.data();

    for (ssize_t row = 0; row < data.shape(0); row++) {
        for (ssize_t col = 0; col < data.shape(1); col++) {

            //Estimate the initial parameters for the fit 
            std::vector<double> start_par{0, 0, 0};
            std::vector<double> start_par_err{0, 0, 0};
            std::vector<double> start_cov{0, 0, 0, 0, 0, 0, 0, 0, 0};

            auto e = std::max_element(ptr, ptr + data.shape(2));
            auto idx = std::distance(ptr, e);
            
            start_par[0] = *e;      // For amplitude we use the maximum value
            start_par[1] = x[idx];  // For the mean we use the x value of the maximum value
            
            // For sigma we estimate the fwhm and divide by 2.35
            // assuming equally spaced x values
            auto delta = x[1] - x[0];
            start_par[2] = std::count_if(ptr, ptr + data.shape(2),
                                         [e, delta](double val) { return val > *e / 2; }) * delta / 2.35;

            // if (row == 0 && col == 0) {
            //     fmt::print("start_par: {} {} {}\n", start_par[0], start_par[1], start_par[2]);
            //     for (size_t i = 0; i < data.shape(2); i++) {
            //         fmt::print("data({},{},{}): {} {}\n", row, col, i, x[i], data(row, col, i));
            //     }
            // }

            lmfit::result_t res(start_par);
            lmfit::result_t res_err(start_par_err);
            lmfit::result_t cov(start_cov);
            
            lmcurve2(res.par.size(), res.par.data(), res_err.par.data(), cov.par.data(), x.size(), x.data(),
                    &data(row, col,0), &data_err(row, col,0), aare::func::gauss, &control, &res.status);
            // fmt::print("par: {} {} {}\n", res.par[0], res.par[1], res.par[2]);

            result(row, col, 0) = res.par[0];
            result(row, col, 1) = res.par[1];
            result(row, col, 2) = res.par[2];
            result(row, col, 3) = res_err.par[0];
            result(row, col, 4) = res_err.par[1];
            result(row, col, 5) = res_err.par[2];
            ptr += data.shape(2);
        }

    }
    return result;
}

// With errors
NDArray<double, 1> fit_gaus2(NDView<double, 1> data, NDView<double, 1> x, NDView<double, 1> data_err) {
    // std::vector<double> start_par{300, 3300, 50};

    NDArray<double, 1> result({6}, 0);
    // NDArray<double, 1> result_err({3}, 0);
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
// ---- AFFINE FUNCTION ----
NDArray<double, 3> fit_affine(NDView<double, 3> data, NDView<double, 1> x, NDView<double, 3> data_err) {
    // std::vector<double> start_par{300, 3300, 50};

    NDArray<double, 3> result({data.shape(0), data.shape(1), 4}, 0);
    // NDArray<double, 3> result_error({data.shape(0), data.shape(1), 3}, 0);

    lm_control_struct control = lm_control_double;

    // fmt::print("data.shape(0): {}\n", data.shape(0));
    // fmt::print("data.shape(1): {}\n", data.shape(1));
    // fmt::print("data.shape(2): {}\n", data.shape(2));

    auto ptr = data.data();

    for (ssize_t row = 0; row < data.shape(0); row++) {
        for (ssize_t col = 0; col < data.shape(1); col++) {

            //Estimate the initial parameters for the fit 
            std::vector<double> start_par{0, 0};
            std::vector<double> start_par_err{0, 0};
            std::vector<double> start_cov{0, 0, 0, 0};



            auto y2 = std::max_element(ptr, ptr + data.shape(2));
            auto x2 = x[std::distance(ptr, y2)];
               
            auto y1 = std::min_element(ptr, ptr + data.shape(2));
            auto x1 = x[std::distance(ptr, y1)];
            
            start_par[0] = (*y2 - *y1) / (x2 - x1);      // For amplitude we use the maximum value
            start_par[1] = *y1 - ((*y2 - *y1) / (x2 - x1)) * x1;  // For the mean we use the x value of the maximum value
            

            lmfit::result_t res(start_par);
            lmfit::result_t res_err(start_par_err);
            lmfit::result_t cov(start_cov);
            
            lmcurve2(res.par.size(), res.par.data(), res_err.par.data(), cov.par.data(), x.size(), x.data(),
                    &data(row, col,0), &data_err(row, col,0), aare::func::affine, &control, &res.status);
            // fmt::print("par: {} {} {}\n", res.par[0], res.par[1], res.par[2]);

            result(row, col, 0) = res.par[0];
            result(row, col, 1) = res.par[1];
            result(row, col, 2) = res_err.par[0];
            result(row, col, 3) = res_err.par[1];
            ptr += data.shape(2);
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