#pragma once

#include "aare/Cluster.hpp"
#include "aare/ClusterVector.hpp"
#include "aare/NDArray.hpp"
#include "aare/defs.hpp"

namespace aare {

enum class pixel : int {
    pBottomLeft = 0,
    pBottom = 1,
    pBottomRight = 2,
    pLeft = 3,
    pCenter = 4,
    pRight = 5,
    pTopLeft = 6,
    pTop = 7,
    pTopRight = 8
};

template <typename T> struct Eta2 {
    double x;
    double y;
    corner c{0};
    T sum;
};

/**
 * @brief Calculate the eta2 values for all clusters in a Clustervector
 */
template <typename ClusterType,
          typename = std::enable_if_t<is_cluster_v<ClusterType>>>
NDArray<double, 2> calculate_eta2(const ClusterVector<ClusterType> &clusters) {
    NDArray<double, 2> eta2({static_cast<int64_t>(clusters.size()), 2});

    for (size_t i = 0; i < clusters.size(); i++) {
        auto e = calculate_eta2(clusters[i]);
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

    static_assert(ClusterSizeX > 1 && ClusterSizeY > 1);
    Eta2<T> eta{};

    size_t cluster_center_index =
        (ClusterSizeX / 2) + (ClusterSizeY / 2) * ClusterSizeX;

    auto max_sum = cl.max_sum_2x2();
    eta.sum = max_sum.sum;
    corner c = max_sum.index;

    // subcluster top right from center
    switch (c) {
    case (corner::cTopLeft):
        if ((cl.data[cluster_center_index - 1] +
             cl.data[cluster_center_index]) != 0)
            eta.x = static_cast<double>(cl.data[cluster_center_index - 1]) /
                    static_cast<double>(cl.data[cluster_center_index - 1] +
                                        cl.data[cluster_center_index]);
        if ((cl.data[cluster_center_index - ClusterSizeX] +
             cl.data[cluster_center_index]) != 0)
            eta.y = static_cast<double>(
                        cl.data[cluster_center_index - ClusterSizeX]) /
                    static_cast<double>(
                        cl.data[cluster_center_index - ClusterSizeX] +
                        cl.data[cluster_center_index]);

        // dx = 0
        // dy = 0
        break;
    case (corner::cTopRight):
        if (cl.data[cluster_center_index] + cl.data[cluster_center_index + 1] !=
            0)
            eta.x = static_cast<double>(cl.data[cluster_center_index]) /
                    static_cast<double>(cl.data[cluster_center_index] +
                                        cl.data[cluster_center_index + 1]);
        if ((cl.data[cluster_center_index - ClusterSizeX] +
             cl.data[cluster_center_index]) != 0)
            eta.y = static_cast<double>(
                        cl.data[cluster_center_index - ClusterSizeX]) /
                    static_cast<double>(
                        cl.data[cluster_center_index - ClusterSizeX] +
                        cl.data[cluster_center_index]);
        // dx = 1
        // dy = 0
        break;
    case (corner::cBottomLeft):
        if ((cl.data[cluster_center_index - 1] +
             cl.data[cluster_center_index]) != 0)
            eta.x = static_cast<double>(cl.data[cluster_center_index - 1]) /
                    static_cast<double>(cl.data[cluster_center_index - 1] +
                                        cl.data[cluster_center_index]);
        if ((cl.data[cluster_center_index] +
             cl.data[cluster_center_index + ClusterSizeX]) != 0)
            eta.y = static_cast<double>(cl.data[cluster_center_index]) /
                    static_cast<double>(
                        cl.data[cluster_center_index] +
                        cl.data[cluster_center_index + ClusterSizeX]);
        // dx = 0
        // dy = 1
        break;
    case (corner::cBottomRight):
        if (cl.data[cluster_center_index] + cl.data[cluster_center_index + 1] !=
            0)
            eta.x = static_cast<double>(cl.data[cluster_center_index]) /
                    static_cast<double>(cl.data[cluster_center_index] +
                                        cl.data[cluster_center_index + 1]);
        if ((cl.data[cluster_center_index] +
             cl.data[cluster_center_index + ClusterSizeX]) != 0)
            eta.y = static_cast<double>(cl.data[cluster_center_index]) /
                    static_cast<double>(
                        cl.data[cluster_center_index] +
                        cl.data[cluster_center_index + ClusterSizeX]);
        // dx = 1
        // dy = 1
        break;
    }

    eta.c = c;

    return eta;
}

// TODO! Look up eta2 calculation - photon center should be bottom right corner
template <typename T>
Eta2<T> calculate_eta2(const Cluster<T, 2, 2, int16_t> &cl) {
    Eta2<T> eta{};

    if ((cl.data[0] + cl.data[1]) != 0)
        eta.x = static_cast<double>(cl.data[2]) /
                (cl.data[2] + cl.data[3]); // between (0,1) the closer to zero
                                           // left value probably larger
    if ((cl.data[0] + cl.data[2]) != 0)
        eta.y = static_cast<double>(cl.data[1]) /
                (cl.data[1] + cl.data[3]); // between (0,1) the closer to zero
                                           // bottom value probably larger
    eta.sum = cl.sum();

    return eta;
}

// TODO generalize
template <typename T>
Eta2<T> calculate_eta2(const Cluster<T, 1, 2, int16_t> &cl) {
    Eta2<T> eta{};

    eta.x = 0;
    eta.y = static_cast<double>(cl.data[0]) / cl.data[1];
    eta.sum = cl.sum();
}

template <typename T>
Eta2<T> calculate_eta2(const Cluster<T, 2, 1, int16_t> &cl) {
    Eta2<T> eta{};

    eta.x = static_cast<double>(cl.data[0]) / cl.data[1];
    eta.y = 0;
    eta.sum = cl.sum();
}

// calculates Eta3 for 3x3 cluster based on code from analyze_cluster
// TODO only supported for 3x3 Clusters
template <typename T> Eta2<T> calculate_eta3(const Cluster<T, 3, 3> &cl) {

    Eta2<T> eta{};

    T sum = 0;

    std::for_each(std::begin(cl.data), std::end(cl.data),
                  [&sum](T x) { sum += x; });

    eta.sum = sum;

    if ((cl.data[3] + cl.data[4] + cl.data[5]) != 0)

        eta.x = static_cast<double>(-cl.data[3] + cl.data[3 + 2]) /

                (cl.data[3] + cl.data[4] + cl.data[5]); // (-1,1)

    if ((cl.data[1] + cl.data[4] + cl.data[7]) != 0)

        eta.y = static_cast<double>(-cl.data[1] + cl.data[2 * 3 + 1]) /

                (cl.data[1] + cl.data[4] + cl.data[7]);

    return eta;
}

} // namespace aare