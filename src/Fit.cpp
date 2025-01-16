#include "aare/Fit.hpp"
#include <lmfit.hpp>

namespace aare {

namespace func {

double gauss(const double x, const double *par) {
    return par[0] * exp(-pow(x - par[1], 2) / (2 * pow(par[2], 2)));
}

} // namespace func

NDArray<double, 3> fit_gaus(NDView<double, 3> data, NDView<double, 1> x) {
    // std::vector<double> start_par{300, 3300, 50};

    NDArray<double, 3> result({data.shape(0), data.shape(1), 3}, 0);

    // std::vector<double> t{
    //     0.,         0.34482759, 0.68965517, 1.03448276, 1.37931034, 1.72413793,
    //     2.06896552, 2.4137931,  2.75862069, 3.10344828, 3.44827586, 3.79310345,
    //     4.13793103, 4.48275862, 4.82758621, 5.17241379, 5.51724138, 5.86206897,
    //     6.20689655, 6.55172414, 6.89655172, 7.24137931, 7.5862069,  7.93103448,
    //     8.27586207, 8.62068966, 8.96551724, 9.31034483, 9.65517241, 10.};

    // std::vector<double> y{
    //     4.54076159e-02, 1.24086210e-01, 3.07355119e-01, 6.90048299e-01,
    //     1.40423773e+00, 2.59014392e+00, 4.33041246e+00, 6.56230997e+00,
    //     9.01376699e+00, 1.12222005e+01, 1.26640270e+01, 1.29535183e+01,
    //     1.20095233e+01, 1.00922012e+01, 7.68719936e+00, 5.30728607e+00,
    //     3.32123005e+00, 1.88385525e+00, 9.68541451e-01, 4.51347460e-01,
    //     1.90645212e-01, 7.29899241e-02, 2.53292325e-02, 7.96715487e-03,
    //     2.27146786e-03, 5.86991813e-04, 1.37492688e-04, 2.91910198e-05,
    //     5.61747364e-06, 9.79839466e-07};

    // lm_control_struct control;
    // memset(&control, 0, sizeof(control));
    // control.ftol = 1e-10;
    // control.xtol = 1e-10;
    // control.gtol = 1e-10;
    // control.epsilon = 1e-10;
    // control.stepbound = 100;
    // control.patience = 1000;
    // control.scale_diag = 1;

    lm_control_struct control = lm_control_double;

    fmt::print("data.shape(0): {}\n", data.shape(0));
    fmt::print("data.shape(1): {}\n", data.shape(1));
    fmt::print("data.shape(2): {}\n", data.shape(2));

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

            if (row == 0 && col == 0) {
                fmt::print("start_par: {} {} {}\n", start_par[0], start_par[1], start_par[2]);
                for (size_t i = 0; i < data.shape(2); i++) {
                    fmt::print("data({},{},{}): {} {}\n", row, col, i, x[i], data(row, col, i));
                }
            }

            lmfit::result_t res(start_par);
            lmcurve(res.par.size(), res.par.data(), x.size(), x.data(),
                    &data(row, col,0), aare::func::gauss, &control, &res.status);
            // fmt::print("par: {} {} {}\n", res.par[0], res.par[1], res.par[2]);

            result(row, col, 0) = res.par[0];
            result(row, col, 1) = res.par[1];
            result(row, col, 2) = res.par[2];

            ptr += data.shape(2);
        }

    // for (ssize_t row = 0; row < data.shape(0); row++) {
    //     for (ssize_t col = 0; col < data.shape(1); col++) {

    //         //Estimate the initial parameters for the fit 
    //         std::vector<double> start_par{0, 0, 0};
    //         auto e = std::max_element(&data(row, col, 0), &data(row, col, 0) + data.shape(2));
    //         auto idx = std::distance(&data(row, col, 0), e);
            
    //         start_par[0] = *e;      // For amplitude we use the maximum value
    //         start_par[1] = x[idx];  // For the mean we use the x value of the maximum value
            
    //         // For sigma we estimate the fwhm and divide by 2.35
    //         // assuming equally spaced x values
    //         auto delta = x[1] - x[0];
    //         start_par[2] = std::count_if(&data(row, col, 0), &data(row, col, 0) + data.shape(2),
    //                                      [e, delta](double val) { return val > *e / 2; }) * delta / 2.35;

    //         if (row == 0 && col == 0) {
    //             fmt::print("start_par: {} {} {}\n", start_par[0], start_par[1], start_par[2]);
    //             for (size_t i = 0; i < data.shape(2); i++) {
    //                 fmt::print("data({},{},{}): {} {}\n", row, col, i, x[i], data(row, col, i));
    //             }
    //         }

    //         lmfit::result_t res(start_par);
    //         lmcurve(start_par.size(), res.par.data(), x.size(), x.data(),
    //                 &data(row, col,0), aare::func::gauss, &control, &res.status);
    //         // fmt::print("par: {} {} {}\n", res.par[0], res.par[1], res.par[2]);

    //         result(row, col, 0) = res.par[0];
    //         result(row, col, 1) = res.par[1];
    //         result(row, col, 2) = res.par[2];
    //     }
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

} // namespace aare