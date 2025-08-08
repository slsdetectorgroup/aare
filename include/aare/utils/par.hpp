#pragma once
#include <thread>
#include <utility>
#include <vector>

#include "aare/utils/task.hpp"

namespace aare {

template <typename F>
void RunInParallel(F func, const std::vector<std::pair<int, int>> &tasks) {
    // auto tasks = split_task(0, y.shape(0), n_threads);
    std::vector<std::thread> threads;
    for (auto &task : tasks) {
        threads.push_back(std::thread(func, task.first, task.second));
    }
    for (auto &thread : threads) {
        thread.join();
    }
}


template <typename T>
std::vector<NDView<T,3>> make_subviews(NDView<T, 3> &data, ssize_t n_threads) {
    std::vector<NDView<T, 3>> subviews;
    subviews.reserve(n_threads);
    auto limits = split_task(0, data.shape(0), n_threads);
    for (const auto &lim : limits) {
        subviews.push_back(data.sub_view(lim.first, lim.second));
    }
    return subviews;
}

} // namespace aare