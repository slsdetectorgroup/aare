// SPDX-License-Identifier: MPL-2.0
#include "aare/utils/task.hpp"

namespace aare {

std::vector<std::pair<int, int>> split_task(int first, int last,
                                            int n_threads) {
    std::vector<std::pair<int, int>> vec;
    vec.reserve(n_threads);

    int n_items = last - first;

    if (n_threads >= n_items) {
        for (int i = 0; i != n_items; ++i) {
            vec.push_back({first + i, first + i + 1});
        }
        return vec;
    }

    int step = n_items / n_threads;
    for (int i = 0; i != n_threads; ++i) {
        int start = first + step * i;
        int stop = first + step * (i + 1);
        if (i == n_threads - 1)
            stop = last;
        vec.push_back({start, stop});
    }
    return vec;
}

} // namespace aare