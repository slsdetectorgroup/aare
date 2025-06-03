#include "aare/utils/task.hpp"

namespace aare {

std::vector<std::pair<int, int>> split_task(int first, int last,
                                            int n_threads) {
    std::vector<std::pair<int, int>> vec;
    vec.reserve(n_threads);

    int n_frames = last - first;

    if (n_threads >= n_frames) {
        for (int i = 0; i != n_frames; ++i) {
            vec.push_back({i, i + 1});
        }
        return vec;
    }

    int step = (n_frames) / n_threads;
    for (int i = 0; i != n_threads; ++i) {
        int start = step * i;
        int stop = step * (i + 1);
        if (i == n_threads - 1)
            stop = last;
        vec.push_back({start, stop});
    }
    return vec;
}

} // namespace aare