
/************************************************
 * @file Cluster.hpp
 * @short definition of cluster, where CoordType (x,y) give
 * the cluster center coordinates and data the actual cluster data
 * cluster size is given as template parameters
 ***********************************************/

#pragma once

#include <algorithm>
#include <array>
#include <cstdint>
#include <numeric>
#include <type_traits>

namespace aare {

// requires clause c++20 maybe update
template <typename T, uint8_t ClusterSizeX, uint8_t ClusterSizeY,
          typename CoordType = int16_t>
struct Cluster {

    static_assert(std::is_arithmetic_v<T>, "T needs to be an arithmetic type");
    static_assert(std::is_integral_v<CoordType>,
                  "CoordType needs to be an integral type");
    static_assert(ClusterSizeX > 0 && ClusterSizeY > 0,
                  "Cluster sizes must be bigger than zero");

    CoordType x;
    CoordType y;
    std::array<T, ClusterSizeX * ClusterSizeY> data;

    static constexpr uint8_t cluster_size_x = ClusterSizeX;
    static constexpr uint8_t cluster_size_y = ClusterSizeY;
    using value_type = T;
    using coord_type = CoordType;

    T sum() const { return std::accumulate(data.begin(), data.end(), T{}); }

    std::pair<T, int> max_sum_2x2() const {

        if constexpr (cluster_size_x == 3 && cluster_size_y == 3) {
            std::array<T, 4> sum_2x2_subclusters;
            sum_2x2_subclusters[0] = data[0] + data[1] + data[3] + data[4];
            sum_2x2_subclusters[1] = data[1] + data[2] + data[4] + data[5];
            sum_2x2_subclusters[2] = data[3] + data[4] + data[6] + data[7];
            sum_2x2_subclusters[3] = data[4] + data[5] + data[7] + data[8];
            int index = std::max_element(sum_2x2_subclusters.begin(),
                                         sum_2x2_subclusters.end()) -
                        sum_2x2_subclusters.begin();
            return std::make_pair(sum_2x2_subclusters[index], index);
        } else if constexpr (cluster_size_x == 2 && cluster_size_y == 2) {
            return std::make_pair(data[0] + data[1] + data[2] + data[3], 0);
        } else {
            constexpr size_t num_2x2_subclusters =
                (ClusterSizeX - 1) * (ClusterSizeY - 1);

            std::array<T, num_2x2_subclusters> sum_2x2_subcluster;
            for (size_t i = 0; i < ClusterSizeY - 1; ++i) {
                for (size_t j = 0; j < ClusterSizeX - 1; ++j)
                    sum_2x2_subcluster[i * (ClusterSizeX - 1) + j] =
                        data[i * ClusterSizeX + j] +
                        data[i * ClusterSizeX + j + 1] +
                        data[(i + 1) * ClusterSizeX + j] +
                        data[(i + 1) * ClusterSizeX + j + 1];
            }

            int index = std::max_element(sum_2x2_subcluster.begin(),
                                         sum_2x2_subcluster.end()) -
                        sum_2x2_subcluster.begin();
            return std::make_pair(sum_2x2_subcluster[index], index);
        }
    }
};

template<typename T>
Cluster<T, 2, 2, uint16_t> reduce_3x3_to_2x2(const Cluster<T, 3, 3, uint16_t> &c) {
    Cluster<T, 2, 2, uint16_t> result;
    
    auto [s, i] = c.max_sum_2x2();
    switch (i) {
        case 0:
            result.x = c.x-1;
            result.y = c.y+1;
            result.data = {c.data[0], c.data[1], c.data[3], c.data[4]};
            break;
        case 1:
            result.x = c.x;
            result.y = c.y + 1;
            result.data = {c.data[1], c.data[2], c.data[4], c.data[5]};
            break;
        case 2:
            result.x = c.x -1;
            result.y = c.y;
            result.data = {c.data[3], c.data[4], c.data[6], c.data[7]};
            break;
        case 3:
            result.x = c.x;
            result.y = c.y;
            result.data = {c.data[4], c.data[5], c.data[7], c.data[8]};
            break;
    }

    // do some stuff
    return result;
}

template<typename T>
Cluster<T, 3, 3, uint16_t> reduce_5x5_to_3x3(const Cluster<T, 5, 5, uint16_t> &c) {
    Cluster<T, 3, 3, uint16_t> result;

    // Reduce the 5x5 cluster to a 3x3 cluster by selecting the 3x3 block with the highest sum
    std::array<T, 9> sum_3x3_subclusters;

    //Write out the sums in the hope that the compiler can optimize this
    sum_3x3_subclusters[0] = c.data[0] + c.data[1] + c.data[2] + c.data[5] + c.data[6] + c.data[7] + c.data[10] + c.data[11] + c.data[12];
    sum_3x3_subclusters[1] = c.data[1] + c.data[2] + c.data[3] + c.data[6] + c.data[7] + c.data[8] + c.data[11] + c.data[12] + c.data[13];
    sum_3x3_subclusters[2] = c.data[2] + c.data[3] + c.data[4] + c.data[7] + c.data[8] + c.data[9] + c.data[12] + c.data[13] + c.data[14];
    sum_3x3_subclusters[3] = c.data[5] + c.data[6] + c.data[7] + c.data[10] + c.data[11] + c.data[12] + c.data[15] + c.data[16] + c.data[17];
    sum_3x3_subclusters[4] = c.data[6] + c.data[7] + c.data[8] + c.data[11] + c.data[12] + c.data[13] + c.data[16] + c.data[17] + c.data[18];
    sum_3x3_subclusters[5] = c.data[7] + c.data[8] + c.data[9] + c.data[12] + c.data[13] + c.data[14] + c.data[17] + c.data[18] + c.data[19];
    sum_3x3_subclusters[6] = c.data[10] + c.data[11] + c.data[12] + c.data[15] + c.data[16] + c.data[17] + c.data[20] + c.data[21] + c.data[22];
    sum_3x3_subclusters[7] = c.data[11] + c.data[12] + c.data[13] + c.data[16] + c.data[17] + c.data[18] + c.data[21] + c.data[22] + c.data[23];
    sum_3x3_subclusters[8] = c.data[12] + c.data[13] + c.data[14] + c.data[17] + c.data[18] + c.data[19] + c.data[22] + c.data[23] + c.data[24];

    auto index = std::max_element(sum_3x3_subclusters.begin(), sum_3x3_subclusters.end()) - sum_3x3_subclusters.begin();

    switch (index) {
        case 0:
            result.x = c.x - 1;
            result.y = c.y + 1;
            result.data = {c.data[0], c.data[1], c.data[2], c.data[5], c.data[6], c.data[7], c.data[10], c.data[11], c.data[12]};
            break;
        case 1:
            result.x = c.x;
            result.y = c.y + 1;
            result.data = {c.data[1], c.data[2], c.data[3], c.data[6], c.data[7], c.data[8], c.data[11], c.data[12], c.data[13]};
            break;
        case 2:
            result.x = c.x + 1;
            result.y = c.y + 1;
            result.data = {c.data[2], c.data[3], c.data[4], c.data[7], c.data[8], c.data[9], c.data[12], c.data[13], c.data[14]};
            break;
        case 3:
            result.x = c.x - 1;
            result.y = c.y;
            result.data = {c.data[5], c.data[6], c.data[7], c.data[10], c.data[11], c.data[12], c.data[15], c.data[16], c.data[17]};
            break;
        case 4:
            result.x = c.x + 1;
            result.y = c.y;
            result.data = {c.data[6], c.data[7], c.data[8], c.data[11], c.data[12], c.data[13], c.data[16], c.data[17], c.data[18]};
            break;
        case 5:
            result.x = c.x + 1;
            result.y = c.y;
            result.data = {c.data[7], c.data[8], c.data[9], c.data[12], c.data[13], c.data[14], c.data[17], c.data[18], c.data[19]};
            break;
        case 6:
            result.x = c.x + 1;
            result.y = c.y -1;
            result.data = {c.data[10], c.data[11], c.data[12], c.data[15], c.data[16], c.data[17], c.data[20], c.data[21], c.data[22]};
            break;
        case 7:
            result.x = c.x + 1;
            result.y = c.y-1;
            result.data = {c.data[11], c.data[12], c.data[13], c.data[16], c.data[17], c.data[18], c.data[21], c.data[22], c.data[23]};
            break;
        case 8:
            result.x = c.x + 1;
            result.y = c.y-1;
            result.data = {c.data[12], c.data[13], c.data[14], c.data[17], c.data[18], c.data[19], c.data[22], c.data[23], c.data[24]};
            break;
    }

    // do some stuff
    return result;
}

// Type Traits for is_cluster_type
template <typename T>
struct is_cluster : std::false_type {}; // Default case: Not a Cluster

template <typename T, uint8_t X, uint8_t Y, typename CoordType>
struct is_cluster<Cluster<T, X, Y, CoordType>> : std::true_type {}; // Cluster

template <typename T> constexpr bool is_cluster_v = is_cluster<T>::value;

} // namespace aare
