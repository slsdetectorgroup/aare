#include "aare.hpp"
#include <iostream>

using namespace std;
using namespace aare;
#include <algorithm>
#include <chrono>
#include <numeric>
#include <random>
#include <vector>

template <typename T> T variance(const std::vector<T> &vec) {
    const size_t sz = vec.size();
    if (sz <= 1) {
        return 0.0;
    }

    // Calculate the mean
    const T mean = std::accumulate(vec.begin(), vec.end(), 0.0) / sz;

    // Now calculate the variance
    auto variance_func = [&mean, &sz](T accumulator, const T &val) {
        return accumulator + ((val - mean) * (val - mean) / (sz - 1));
    };

    return std::accumulate(vec.begin(), vec.end(), 0.0, variance_func);
}
int range(int min, int max, int i,int steps) { return min + (max - min) * i / steps; }
int main() {
    const int rows = 1, cols = 1;
    const double MEAN = 5.0, STD = 2.0, VAR = STD * STD, TOLERANCE = 0.1;

    unsigned seed = std::chrono::system_clock::now().time_since_epoch().count();
    std::default_random_engine generator(seed);
    std::normal_distribution<double> distribution(MEAN, STD);

    Pedestal pedestal(rows, cols, 1000);
    std::vector<std::pair<int, double>> values, pmean, pvariance, pstandard_deviation;

    int steps = 1000;
    for (int n_iters = 0; n_iters < steps; n_iters++) {
        int x=range(1000, 1000000, n_iters,steps);
        std::cout<<x<<std::endl;
        
        for (int i = 0; i < x; i++) {
            Frame frame(rows, cols, 64);
            for (int j = 0; j < rows; j++)
                for (int k = 0; k < cols; k++) {
                    double val = distribution(generator);
                    frame.set<double>(j, k, val);
                }
            pedestal.push<double>(frame);
        }
        pmean.push_back({x, pedestal.mean(0, 0)});
        pvariance.push_back({x, pedestal.variance(0, 0)});
    }

    std::cout << "MEAN" << std::endl;
    std::cout << "[ ";
    for (int i = 0; i < pmean.size(); i++) {
        std::cout << "(" << pmean[i].first << "," << pmean[i].second << "), ";
    }
    std::cout << "]" << std::endl;
    std::cout << "VAR" << std::endl;
    std::cout << "[ ";
    for (int i = 0; i < pvariance.size(); i++) {
        std::cout << "(" << pvariance[i].first << "," << pvariance[i].second << "), ";
    }
    std::cout << "]" << std::endl;

    // std::cout << "PEDESTAL" << std::endl;
    // std::cout << "Mean: " << pmean(0, 0) << std::endl;
    // std::cout << "Variance: " << pvariance(0, 0) << std::endl;
    // std::cout << "Standard Deviation: " << pstandard_deviation(0, 0) << std::endl;

    // std::cout << "VALUES" << std::endl;
    // std::cout << "Mean: " << std::accumulate(values.begin(), values.end(), 0.0) / values.size() << std::endl;
    // std::cout << "Variance: " << variance<double>(values) << std::endl;
    // std::cout << "Standard Deviation: " << std::sqrt(variance<double>(values)) << std::endl;
}