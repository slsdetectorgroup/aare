#include <thread>
#include <vector>
#include <utility>

namespace aare {

    template<typename F>
    void RunInParallel(F func, const std::vector<std::pair<int, int>>& tasks) {
        // auto tasks = split_task(0, y.shape(0), n_threads);
        std::vector<std::thread> threads;
        for (auto &task : tasks) {
            threads.push_back(std::thread(func, task.first, task.second));
        }
        for (auto &thread : threads) {
            thread.join();
        }
    }
} // namespace aare