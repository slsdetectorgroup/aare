#pragma once

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <numeric>

namespace aare {

//TODO! Template this? 
struct Cluster3x3 {
    int16_t x;
    int16_t y;
    int32_t data[9];

    int32_t sum_2x2() const{
        std::array<int32_t, 4> total;
        total[0] = data[0] + data[1] + data[3] + data[4];
        total[1] = data[1] + data[2] + data[4] + data[5];
        total[2] = data[3] + data[4] + data[6] + data[7];
        total[3] = data[4] + data[5] + data[7] + data[8];
        return *std::max_element(total.begin(), total.end());
    }

    int32_t sum() const{
        return std::accumulate(data, data + 9, 0);
    }
};
struct Cluster2x2 {
    int16_t x;
    int16_t y;
    int32_t data[4];
};

} // namespace aare