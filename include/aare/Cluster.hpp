
/************************************************
 * @file Cluster.hpp
 * @short definition of cluster, where CoordType (x,y) give
 * the cluster center coordinates and data the actual cluster data
 * cluster size is given as template parameters
 ***********************************************/

#pragma once

#include <cstdint>
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
};

// Type Traits for is_cluster_type
template <typename T>
struct is_cluster : std::false_type {}; // Default case: Not a Cluster

template <typename T, uint8_t X, uint8_t Y, typename CoordType>
struct is_cluster<Cluster<T, X, Y, CoordType>> : std::true_type {}; // Cluster

template <typename T> constexpr bool is_cluster_v = is_cluster<T>::value;

} // namespace aare
