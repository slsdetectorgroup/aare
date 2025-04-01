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

// TODO: maybe template this!!!!!! why int32_t????
struct Eta2 {
    double x;
    double y;
    int c;
    int32_t sum;
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
Eta2 calculate_eta2(
    const Cluster<T, ClusterSizeX, ClusterSizeY, CoordType> &cl) {
    Eta2 eta{};

    constexpr size_t num_2x2_subclusters =
        (ClusterSizeX - 1) * (ClusterSizeY - 1);
    std::array<T, num_2x2_subclusters> sum_2x2_subcluster;
    for (size_t i = 0; i < ClusterSizeY - 1; ++i) {
        for (size_t j = 0; j < ClusterSizeX - 1; ++j)
            sum_2x2_subcluster[i * (ClusterSizeX - 1) + j] =
                cl.data[i * ClusterSizeX + j] +
                cl.data[i * ClusterSizeX + j + 1] +
                cl.data[(i + 1) * ClusterSizeX + j] +
                cl.data[(i + 1) * ClusterSizeX + j + 1];
    }

    auto c =
        std::max_element(sum_2x2_subcluster.begin(), sum_2x2_subcluster.end()) -
        sum_2x2_subcluster.begin();

    eta.sum = sum_2x2_subcluster[c];

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

/**
 * @brief Calculate the eta2 values for a 3x3 cluster and return them in a Eta2
 * struct containing etay, etax and the corner of the cluster.
 */
template <typename T> Eta2 calculate_eta2(const Cluster<T, 3, 3> &cl) {
    Eta2 eta{};

    std::array<T, 4> tot2;
    tot2[0] = cl.data[0] + cl.data[1] + cl.data[3] + cl.data[4];
    tot2[1] = cl.data[1] + cl.data[2] + cl.data[4] + cl.data[5];
    tot2[2] = cl.data[3] + cl.data[4] + cl.data[6] + cl.data[7];
    tot2[3] = cl.data[4] + cl.data[5] + cl.data[7] + cl.data[8];

    auto c = std::max_element(tot2.begin(), tot2.end()) - tot2.begin();
    eta.sum = tot2[c];
    switch (c) {
    case cBottomLeft:
        if ((cl.data[3] + cl.data[4]) != 0)
            eta.x = static_cast<double>(cl.data[4]) / (cl.data[3] + cl.data[4]);
        if ((cl.data[1] + cl.data[4]) != 0)
            eta.y = static_cast<double>(cl.data[4]) / (cl.data[1] + cl.data[4]);
        eta.c = cBottomLeft;
        break;
    case cBottomRight:
        if ((cl.data[2] + cl.data[5]) != 0)
            eta.x = static_cast<double>(cl.data[5]) / (cl.data[4] + cl.data[5]);
        if ((cl.data[1] + cl.data[4]) != 0)
            eta.y = static_cast<double>(cl.data[4]) / (cl.data[1] + cl.data[4]);
        eta.c = cBottomRight;
        break;
    case cTopLeft:
        if ((cl.data[7] + cl.data[4]) != 0)
            eta.x = static_cast<double>(cl.data[4]) / (cl.data[3] + cl.data[4]);
        if ((cl.data[7] + cl.data[4]) != 0)
            eta.y = static_cast<double>(cl.data[7]) / (cl.data[7] + cl.data[4]);
        eta.c = cTopLeft;
        break;
    case cTopRight:
        if ((cl.data[5] + cl.data[4]) != 0)
            eta.x = static_cast<double>(cl.data[5]) / (cl.data[5] + cl.data[4]);
        if ((cl.data[7] + cl.data[4]) != 0)
            eta.y = static_cast<double>(cl.data[7]) / (cl.data[7] + cl.data[4]);
        eta.c = cTopRight;
        break;
    }
    return eta;
}

template <typename T> Eta2 calculate_eta2(const Cluster<T, 2, 2> &cl) {
    Eta2 eta{};

    if ((cl.data[0] + cl.data[1]) != 0)
        eta.x = static_cast<double>(cl.data[1]) / (cl.data[0] + cl.data[1]);
    if ((cl.data[0] + cl.data[2]) != 0)
        eta.y = static_cast<double>(cl.data[2]) / (cl.data[0] + cl.data[2]);
    eta.sum = cl.data[0] + cl.data[1] + cl.data[2] + cl.data[3];
    eta.c = cBottomLeft; // TODO! This is not correct, but need to put something
    return eta;
}

// calculates Eta3 for 3x3 cluster based on code from analyze_cluster
// TODO only supported for 3x3 Clusters
template <typename T> Eta2 calculate_eta3(const Cluster<T, 3, 3> &cl) {

    Eta2 eta{};

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