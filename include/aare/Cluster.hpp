
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
          typename CoordType = uint16_t>
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

    // TODO: handle 1 dimensional clusters
    // TODO: change int to corner
    /**
     * @brief sum of 2x2 subcluster with highest energy
     * @return photon energy of subcluster, 2x2 subcluster index relative to
     * cluster center
     */
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
            constexpr size_t cluster_center_index =
                (ClusterSizeX / 2) + (ClusterSizeY / 2) * ClusterSizeX;

            std::array<T, 4> sum_2x2_subcluster{0};
            // subcluster top left from center
            sum_2x2_subcluster[0] =
                data[cluster_center_index] + data[cluster_center_index - 1] +
                data[cluster_center_index - ClusterSizeX] +
                data[cluster_center_index - 1 - ClusterSizeX];
            // subcluster top right from center
            if (ClusterSizeX > 2) {
                sum_2x2_subcluster[1] =
                    data[cluster_center_index] +
                    data[cluster_center_index + 1] +
                    data[cluster_center_index - ClusterSizeX] +
                    data[cluster_center_index - ClusterSizeX + 1];
            }
            // subcluster bottom left from center
            if (ClusterSizeY > 2) {
                sum_2x2_subcluster[2] =
                    data[cluster_center_index] +
                    data[cluster_center_index - 1] +
                    data[cluster_center_index + ClusterSizeX] +
                    data[cluster_center_index + ClusterSizeX - 1];
            }
            // subcluster bottom right from center
            if (ClusterSizeX > 2 && ClusterSizeY > 2) {
                sum_2x2_subcluster[3] =
                    data[cluster_center_index] +
                    data[cluster_center_index + 1] +
                    data[cluster_center_index + ClusterSizeX] +
                    data[cluster_center_index + ClusterSizeX + 1];
            }

            int index = std::max_element(sum_2x2_subcluster.begin(),
                                         sum_2x2_subcluster.end()) -
                        sum_2x2_subcluster.begin();
            return std::make_pair(sum_2x2_subcluster[index], index);
        }
    }
};

/**
 * @brief Reduce a cluster to a 2x2 cluster by selecting the 2x2 block with the
 * highest sum.
 * @param c Cluster to reduce
 * @return reduced cluster
 */
template <typename T, uint8_t ClusterSizeX, uint8_t ClusterSizeY,
          typename CoordType = int16_t>
Cluster<T, 2, 2, CoordType>
reduce_to_2x2(const Cluster<T, ClusterSizeX, ClusterSizeY, CoordType> &c) {

    static_assert(ClusterSizeX >= 2 && ClusterSizeY >= 2,
                  "Cluster sizes must be at least 2x2 for reduction to 2x2");

    // TODO maybe add sanity check and check that center is in max subcluster
    Cluster<T, 2, 2, CoordType> result;

    auto [sum, index] = c.max_sum_2x2();

    int16_t cluster_center_index =
        (ClusterSizeX / 2) + (ClusterSizeY / 2) * ClusterSizeX;

    int16_t index_bottom_left_max_2x2_subcluster =
        (int(index / (ClusterSizeX - 1))) * ClusterSizeX +
        index % (ClusterSizeX - 1);

    result.x =
        c.x + (index_bottom_left_max_2x2_subcluster - cluster_center_index) %
                  ClusterSizeX;

    result.y =
        c.y - (index_bottom_left_max_2x2_subcluster - cluster_center_index) /
                  ClusterSizeX;
    result.data = {
        c.data[index_bottom_left_max_2x2_subcluster],
        c.data[index_bottom_left_max_2x2_subcluster + 1],
        c.data[index_bottom_left_max_2x2_subcluster + ClusterSizeX],
        c.data[index_bottom_left_max_2x2_subcluster + ClusterSizeX + 1]};
    return result;
}

template <typename T>
Cluster<T, 2, 2, int16_t> reduce_to_2x2(const Cluster<T, 3, 3, int16_t> &c) {
    Cluster<T, 2, 2, int16_t> result;

    auto [s, i] = c.max_sum_2x2();
    switch (i) {
    case 0:
        result.x = c.x - 1;
        result.y = c.y + 1;
        result.data = {c.data[0], c.data[1], c.data[3], c.data[4]};
        break;
    case 1:
        result.x = c.x;
        result.y = c.y + 1;
        result.data = {c.data[1], c.data[2], c.data[4], c.data[5]};
        break;
    case 2:
        result.x = c.x - 1;
        result.y = c.y;
        result.data = {c.data[3], c.data[4], c.data[6], c.data[7]};
        break;
    case 3:
        result.x = c.x;
        result.y = c.y;
        result.data = {c.data[4], c.data[5], c.data[7], c.data[8]};
        break;
    }

    return result;
}

template <typename T, uint8_t ClusterSizeX, uint8_t ClusterSizeY,
          typename CoordType = int16_t>
inline std::pair<T, uint16_t>
max_3x3_sum(const Cluster<T, ClusterSizeX, ClusterSizeY, CoordType> &cluster) {

    if constexpr (ClusterSizeX == 3 && ClusterSizeY == 3) {
        return std::make_pair(cluster.sum(), 0);
    } else {

        size_t index = 0;
        T max_3x3_subcluster_sum = 0;
        for (size_t i = 0; i < ClusterSizeY - 2; ++i) {
            for (size_t j = 0; j < ClusterSizeX - 2; ++j) {

                T sum = cluster.data[i * ClusterSizeX + j] +
                        cluster.data[i * ClusterSizeX + j + 1] +
                        cluster.data[i * ClusterSizeX + j + 2] +
                        cluster.data[(i + 1) * ClusterSizeX + j] +
                        cluster.data[(i + 1) * ClusterSizeX + j + 1] +
                        cluster.data[(i + 1) * ClusterSizeX + j + 2] +
                        cluster.data[(i + 2) * ClusterSizeX + j] +
                        cluster.data[(i + 2) * ClusterSizeX + j + 1] +
                        cluster.data[(i + 2) * ClusterSizeX + j + 2];
                if (sum > max_3x3_subcluster_sum) {
                    max_3x3_subcluster_sum = sum;
                    index = i * (ClusterSizeX - 2) + j;
                }
            }
        }

        return std::make_pair(max_3x3_subcluster_sum, index);
    }
}

/**
 * @brief Reduce a cluster to a 3x3 cluster by selecting the 3x3 block with the
 * highest sum.
 * @param c Cluster to reduce
 * @return reduced cluster
 */
template <typename T, uint8_t ClusterSizeX, uint8_t ClusterSizeY,
          typename CoordType = int16_t>
Cluster<T, 3, 3, CoordType>
reduce_to_3x3(const Cluster<T, ClusterSizeX, ClusterSizeY, CoordType> &c) {

    static_assert(ClusterSizeX >= 3 && ClusterSizeY >= 3,
                  "Cluster sizes must be at least 3x3 for reduction to 3x3");

    Cluster<T, 3, 3, CoordType> result;

    // TODO maybe add sanity check and check that center is in max subcluster

    auto [sum, index] = max_3x3_sum(c);

    int16_t cluster_center_index =
        (ClusterSizeX / 2) + (ClusterSizeY / 2) * ClusterSizeX;

    int16_t index_center_max_3x3_subcluster =
        (int(index / (ClusterSizeX - 2))) * ClusterSizeX + ClusterSizeX +
        index % (ClusterSizeX - 2) + 1;

    int16_t index_3x3_subcluster_cluster_center =
        int((cluster_center_index - 1 - ClusterSizeX) / ClusterSizeX) *
            (ClusterSizeX - 2) +
        (cluster_center_index - 1 - ClusterSizeX) % ClusterSizeX;

    result.x =
        c.x + (index % (ClusterSizeX - 2) -
               (index_3x3_subcluster_cluster_center % (ClusterSizeX - 2)));
    result.y =
        c.y - (index / (ClusterSizeX - 2) -
               (index_3x3_subcluster_cluster_center / (ClusterSizeX - 2)));

    result.data = {c.data[index_center_max_3x3_subcluster - ClusterSizeX - 1],
                   c.data[index_center_max_3x3_subcluster - ClusterSizeX],
                   c.data[index_center_max_3x3_subcluster - ClusterSizeX + 1],
                   c.data[index_center_max_3x3_subcluster - 1],
                   c.data[index_center_max_3x3_subcluster],
                   c.data[index_center_max_3x3_subcluster + 1],
                   c.data[index_center_max_3x3_subcluster + ClusterSizeX - 1],
                   c.data[index_center_max_3x3_subcluster + ClusterSizeX],
                   c.data[index_center_max_3x3_subcluster + ClusterSizeX + 1]};
    return result;
}

// Type Traits for is_cluster_type
template <typename T>
struct is_cluster : std::false_type {}; // Default case: Not a Cluster

template <typename T, uint8_t X, uint8_t Y, typename CoordType>
struct is_cluster<Cluster<T, X, Y, CoordType>> : std::true_type {}; // Cluster

template <typename T> constexpr bool is_cluster_v = is_cluster<T>::value;

} // namespace aare
