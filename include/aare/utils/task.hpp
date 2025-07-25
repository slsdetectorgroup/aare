#pragma once
#include <utility>
#include <vector>

namespace aare {
std::vector<std::pair<int, int>> split_task(int first, int last, int n_threads);

} // namespace aare