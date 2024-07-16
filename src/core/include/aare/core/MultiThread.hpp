// class that takes std::function as a constructor argument
// and run each of them in different threads

#pragma once

#include <functional>
#include <thread>
#include <vector>

namespace aare {

class MultiThread {
  public:
    explicit MultiThread(std::vector<std::function<void()>> const &functions) : functions_(functions) {}

    void run() {
        std::vector<std::thread> threads;
        for (auto const &f : functions_) {
            threads.emplace_back(f);
        }
        for (auto &t : threads) {
            t.join();
        }
    }

  private:
    std::vector<std::function<void()>> functions_;
};

} // namespace aare