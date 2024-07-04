#include "aare.hpp"
#include <iostream>

using namespace std;
using namespace aare;
#include <algorithm>
#include <chrono>
#include <numeric>
#include <random>
#include <vector>

void print_vpair(std::vector<std::pair<int, double>> &v) {
    std::cout << "[ ";
    for (unsigned int i = 0; i < v.size(); i++) {
        std::cout << "(" << v[i].first << "," << v[i].second << "), ";
    }
    std::cout << "]" << std::endl;
}
int range(int min, int max, int i, int steps) { return min + (max - min) * i / steps; }
int main() {
    const int rows = 1, cols = 1;
    double MEAN = 5.0, STD = 1.0;

    unsigned seed = std::chrono::system_clock::now().time_since_epoch().count();
    std::default_random_engine generator(seed);
    std::normal_distribution<double> distribution(MEAN, STD);

    Pedestal pedestal(rows, cols, 1000);
    std::vector<double> values;
    std::vector<double> vmean, vvariance, pmean, pvariance, pstandard_deviation;
    std::vector<int> samples;
    std::vector<std::pair<int, double>> cur_mean, cur_variance;

    // int steps = 1000;
    // for (int n_iters = 0; n_iters < steps; n_iters++) {
    //     int x=range(1000, 1000000, n_iters,steps);
    //     std::cout<<x<<std::endl;

    //     for (int i = 0; i < x; i++) {
    //         Frame frame(rows, cols, 64);
    //         for (int j = 0; j < rows; j++)
    //             for (int k = 0; k < cols; k++) {
    //                 double val = distribution(generator);
    //                 frame.set<double>(j, k, val);
    //             }
    //         pedestal.push<double>(frame);
    //     }
    //     pmean.push_back({x, pedestal.mean(0, 0)});
    //     pvariance.push_back({x, pedestal.variance(0, 0)});
    // }

    long double sum = 0;
    long double sum2 = 0;
    // fill 1000 first values of pedestal
    for (int x = 0; x < 1000; x++) {
        Frame frame(rows, cols, Dtype::DOUBLE);
        double val = distribution(generator);
        frame.set<double>(0, 0, val);
        pedestal.push<double>(frame);
        values.push_back(val);
        sum += val;
        sum2 += val * val;
    }

    for (int x = 0, aa = 0; x < 100000; x++, aa++) {
        Frame frame(rows, cols, Dtype::DOUBLE);
        double val = distribution(generator);
        frame.set<double>(0, 0, val);
        pedestal.push<double>(frame);

        values.push_back(val);
        auto old_val = values[values.size() - 1000 - 1];
        sum += val - old_val;
        sum2 += val * val - old_val * old_val;
        if (aa % 100 == 1) {
            aa = 2;
            samples.push_back(x);
            vmean.push_back(sum / 1000);
            vvariance.push_back(sum2 / (1000) - (sum / (1000)) * (sum / (1000)));
            pmean.push_back(pedestal.mean(0, 0));
            pvariance.push_back(pedestal.variance(0, 0));
        }
        if (x % 1000 == 999) {
            MEAN *= 1.1;
            STD *= 1.1;

            distribution.param(std::normal_distribution<double>::param_type(MEAN, STD));
            cur_mean.push_back({x, MEAN});
            cur_variance.push_back({x, STD * STD});
        }
    }

    logger::info("x6=", samples);
    logger::info("pmean6=", pmean);
    logger::info("pvar6=", pvariance);
    logger::info("vmean6=", vmean);
    logger::info("vvar6=", vvariance);
    std::cout << "cur_mean6=";
    print_vpair(cur_mean);
    std::cout << "cur_variance6=";
    print_vpair(cur_variance);

    // std::cout << "PEDESTAL" << std::endl;
    // std::cout << "Mean: " << pmean(0, 0) << std::endl;
    // std::cout << "Variance: " << pvariance(0, 0) << std::endl;
    // std::cout << "Standard Deviation: " << pstandard_deviation(0, 0) << std::endl;

    // std::cout << "VALUES" << std::endl;
    // std::cout << "Mean: " << std::accumulate(values.begin(), values.end(), 0.0) / values.size() << std::endl;
    // std::cout << "Variance: " << variance<double>(values) << std::endl;
    // std::cout << "Standard Deviation: " << std::sqrt(variance<double>(values)) << std::endl;
}