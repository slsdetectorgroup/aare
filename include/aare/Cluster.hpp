
/************************************************
 * @file Cluster.hpp
 * @short definition of cluster, where CoordType (x,y) give
 * the cluster center coordinates and data the actual cluster data
 * cluster size is given as template parameters
 ***********************************************/

#pragma once

#include <algorithm>
#include <cstdint>
#include <numeric>
#include <type_traits>

namespace aare {

template <typename T, uint8_t ClusterSizeX, uint8_t ClusterSizeY,
          typename CoordType = int16_t>
constexpr bool is_valid_cluster =
    std::is_arithmetic_v<T> && std::is_integral_v<CoordType> &&
    (ClusterSizeX > 0) && (ClusterSizeY > 0);

// requires clause c++20 maybe update
template <typename T, uint8_t ClusterSizeX, uint8_t ClusterSizeY,
          typename CoordType = int16_t,
          typename Enable = std::enable_if_t<
              is_valid_cluster<T, ClusterSizeX, ClusterSizeY, CoordType>>>
struct Cluster {
    CoordType x;
    CoordType y;
    T data[ClusterSizeX * ClusterSizeY];

    T sum() const {
        return std::accumulate(data, data + ClusterSizeX * ClusterSizeY, 0);
    }

    std::pair<T, int> max_sum_2x2() const {

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
};

// Specialization for 2x2 clusters (only one sum exists)
template <typename T> struct Cluster<T, 2, 2, int16_t> {
    int16_t x;
    int16_t y;
    T data[4];

    T sum() const { return std::accumulate(data, data + 4, 0); }

    std::pair<T, int> max_sum_2x2() const {
        return std::make_pair(data[0] + data[1] + data[2] + data[3],
                              0); // Only one possible 2x2 sum
    }
};

// Specialization for 3x3 clusters
template <typename T> struct Cluster<T, 3, 3, int16_t> {
    int16_t x;
    int16_t y;
    T data[9];

    T sum() const { return std::accumulate(data, data + 9, 0); }

    std::pair<T, int> max_sum_2x2() const {
        std::array<T, 4> sum_2x2_subclusters;
        sum_2x2_subclusters[0] = data[0] + data[1] + data[3] + data[4];
        sum_2x2_subclusters[1] = data[1] + data[2] + data[4] + data[5];
        sum_2x2_subclusters[2] = data[3] + data[4] + data[6] + data[7];
        sum_2x2_subclusters[3] = data[4] + data[5] + data[7] + data[8];
        int index = std::max_element(sum_2x2_subclusters.begin(),
                                     sum_2x2_subclusters.end()) -
                    sum_2x2_subclusters.begin();
        return std::make_pair(sum_2x2_subclusters[index], index);
    }
};

// Type Traits for is_cluster_type
template <typename T>
struct is_cluster : std::false_type {}; // Default case: Not a Cluster

template <typename T, uint8_t X, uint8_t Y, typename CoordType>
struct is_cluster<Cluster<T, X, Y, CoordType>> : std::true_type {}; // Cluster

template <typename T> constexpr bool is_cluster_v = is_cluster<T>::value;

} // namespace aare
