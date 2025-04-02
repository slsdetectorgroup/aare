#pragma once

#include "aare/Cluster.hpp"
#include "aare/ClusterVector.hpp"
#include "aare/NDArray.hpp"

namespace aare {

typedef enum {
    cBottomLeft = 0,
    cBottomRight = 1,
    cTopLeft = 2,
    cTopRight = 3
} corner;

typedef enum {
    pBottomLeft = 0,
    pBottom = 1,
    pBottomRight = 2,
    pLeft = 3,
    pCenter = 4,
    pRight = 5,
    pTopLeft = 6,
    pTop = 7,
    pTopRight = 8
} pixel;

template <typename T> struct Eta2 {
    double x;
    double y;
    int c;
    T sum;
};

/**
 * @brief Calculate the eta2 values for all clusters in a Clsutervector
 */
template <typename ClusterType, std::enable_if_t<is_cluster_v<ClusterType>>>
NDArray<double, 2> calculate_eta2(const ClusterVector<ClusterType> &clusters) {
    NDArray<double, 2> eta2({static_cast<int64_t>(clusters.size()), 2});

    for (size_t i = 0; i < clusters.size(); i++) {
        auto e = calculate_eta2(clusters.at(i));
        eta2(i, 0) = e.x;
        eta2(i, 1) = e.y;
    }

    return eta2;
}

/**
 * @brief Calculate the eta2 values for a generic sized cluster and return them
 * in a Eta2 struct containing etay, etax and the index of the respective 2x2
 * subcluster.
 */
template <typename T, uint8_t ClusterSizeX, uint8_t ClusterSizeY,
          typename CoordType>
Eta2<T>
calculate_eta2(const Cluster<T, ClusterSizeX, ClusterSizeY, CoordType> &cl) {
    Eta2<T> eta{};

    auto max_sum = cl.max_sum_2x2();
    eta.sum = max_sum.first;
    auto c = max_sum.second;

    size_t index_bottom_left_max_2x2_subcluster =
        (int(c / (ClusterSizeX - 1))) * ClusterSizeX + c % (ClusterSizeX - 1);

    if ((cl.data[index_bottom_left_max_2x2_subcluster] +
         cl.data[index_bottom_left_max_2x2_subcluster + 1]) != 0)
        eta.x = static_cast<double>(
                    cl.data[index_bottom_left_max_2x2_subcluster + 1]) /
                (cl.data[index_bottom_left_max_2x2_subcluster] +
                 cl.data[index_bottom_left_max_2x2_subcluster + 1]);

    if ((cl.data[index_bottom_left_max_2x2_subcluster] +
         cl.data[index_bottom_left_max_2x2_subcluster + ClusterSizeX]) != 0)
        eta.y =
            static_cast<double>(
                cl.data[index_bottom_left_max_2x2_subcluster + ClusterSizeX]) /
            (cl.data[index_bottom_left_max_2x2_subcluster] +
             cl.data[index_bottom_left_max_2x2_subcluster + ClusterSizeX]);

    eta.c = c; // TODO only supported for 2x2 and 3x3 clusters -> at least no
               // underyling enum class
    return eta;
}

// calculates Eta3 for 3x3 cluster based on code from analyze_cluster
// TODO only supported for 3x3 Clusters
template <typename T> Eta2<T> calculate_eta3(const Cluster<T, 3, 3> &cl) {

    Eta2<T> eta{};

    T sum = 0;

    std::for_each(std::begin(cl.data), std::end(cl.data),
                  [&sum](T x) { sum += x; });

    eta.sum = sum;

    eta.c = corner::cBottomLeft;

    if ((cl.data[3] + cl.data[4] + cl.data[5]) != 0)

        eta.x = static_cast<double>(-cl.data[3] + cl.data[3 + 2]) /

                (cl.data[3] + cl.data[4] + cl.data[5]);

    if ((cl.data[1] + cl.data[4] + cl.data[7]) != 0)

        eta.y = static_cast<double>(-cl.data[1] + cl.data[2 * 3 + 1]) /

                (cl.data[1] + cl.data[4] + cl.data[7]);

    return eta;
}

} // namespace aare