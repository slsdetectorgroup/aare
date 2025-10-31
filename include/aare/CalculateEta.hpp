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

/**
 * eta struct
 */
template <typename T> struct Eta2 {
    /// @brief eta in x direction
    double x;
    /// @brief eta in y direction
    double y;
    /// @brief index of subcluster given as corner relative to cluster center
    corner c{0};
    /// @brief photon energy (cluster sum)
    T sum;
};

/**
 * @brief Calculate the eta2 values for all clusters in a ClusterVector
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
 * @brief Calculate the full eta2 values for all clusters in a ClusterVector
 */
template <typename ClusterType,
          typename = std::enable_if_t<is_cluster_v<ClusterType>>>
NDArray<double, 2>
calculate_full_eta2(const ClusterVector<ClusterType> &clusters) {
    NDArray<double, 2> eta2({static_cast<int64_t>(clusters.size()), 2});

    for (size_t i = 0; i < clusters.size(); i++) {
        auto e = calculate_full_eta2(clusters[i]);
        eta2(i, 0) = e.x;
        eta2(i, 1) = e.y;
    }

    return eta2;
}

/**
 * @brief Calculate eta3 for all 3x3 clusters in a ClusterVector
 */
template <
    typename T, typename CoordType,
    typename = std::enable_if_t<is_cluster_v<Cluster<T, 3, 3, CoordType>>>>
NDArray<double, 2>
calculate_eta3(const ClusterVector<Cluster<T, 3, 3, CoordType>> &clusters) {
    NDArray<double, 2> eta({static_cast<int64_t>(clusters.size()), 2});

    for (size_t i = 0; i < clusters.size(); i++) {
        auto e = calculate_eta3(clusters[i]);
        eta(i, 0) = e.x;
        eta(i, 1) = e.y;
    }

    return eta;
}

/**
 * @brief Calculate cross eta3 for all 3x3 clusters in a ClusterVector
 */
template <
    typename T, typename CoordType,
    typename = std::enable_if_t<is_cluster_v<Cluster<T, 3, 3, CoordType>>>>
NDArray<double, 2> calculate_cross_eta3(
    const ClusterVector<Cluster<T, 3, 3, CoordType>> &clusters) {
    NDArray<double, 2> eta({static_cast<int64_t>(clusters.size()), 2});

    for (size_t i = 0; i < clusters.size(); i++) {
        auto e = calculate_cross_eta3(clusters[i]);
        eta(i, 0) = e.x;
        eta(i, 1) = e.y;
    }

    return eta;
}

/**
 * @brief Calculate the eta2 values for a generic sized cluster and return them
 * in a Eta2 struct containing etay, etax and the index (as corner) of the
 * respective 2x2 subcluster relative to the cluster center.
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

/**
 * @brief Calculate the eta2 values for a generic sized cluster and return them
 * in a Eta2 struct containing etay, etax and the index (as corner) of the
 * respective 2x2 subcluster relative to the cluster center.
 */
template <typename T, uint8_t ClusterSizeX, uint8_t ClusterSizeY,
          typename CoordType>
Eta2<T> calculate_full_eta2(
    const Cluster<T, ClusterSizeX, ClusterSizeY, CoordType> &cl) {

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
        if (eta.sum != 0) {
            eta.x = static_cast<double>(
                        cl.data[cluster_center_index - 1] +
                        cl.data[cluster_center_index - 1 - ClusterSizeX]) /
                    static_cast<double>(eta.sum);

            eta.y = static_cast<double>(
                        cl.data[cluster_center_index - 1 - ClusterSizeX] +
                        cl.data[cluster_center_index - ClusterSizeX]) /
                    static_cast<double>(eta.sum);
        }
        // dx = 0
        // dy = 0
        break;
    case (corner::cTopRight):
        if (eta.sum != 0) {
            eta.x = static_cast<double>(
                        cl.data[cluster_center_index] +
                        cl.data[cluster_center_index - ClusterSizeX]) /
                    static_cast<double>(eta.sum);
            eta.y = static_cast<double>(
                        cl.data[cluster_center_index - ClusterSizeX] +
                        cl.data[cluster_center_index - ClusterSizeX + 1]) /
                    static_cast<double>(eta.sum);
        }
        // dx = 1
        // dy = 0
        break;
    case (corner::cBottomLeft):
        if (eta.sum != 0) {
            eta.x = static_cast<double>(
                        cl.data[cluster_center_index - 1] +
                        cl.data[cluster_center_index - 1 + ClusterSizeX]) /
                    static_cast<double>(eta.sum);
            eta.y = static_cast<double>(cl.data[cluster_center_index - 1] +
                                        cl.data[cluster_center_index]) /
                    static_cast<double>(eta.sum);
        }
        // dx = 0
        // dy = 1
        break;
    case (corner::cBottomRight):
        if (eta.sum != 0) {
            eta.x = static_cast<double>(
                        cl.data[cluster_center_index] +
                        cl.data[cluster_center_index + ClusterSizeX]) /
                    static_cast<double>(eta.sum);
            eta.y = static_cast<double>(cl.data[cluster_center_index] +
                                        cl.data[cluster_center_index + 1]) /
                    static_cast<double>(eta.sum);
        }
        // dx = 1
        // dy = 1
        break;
    }

    eta.c = c;

    return eta;
}

template <typename T>
Eta2<T> calculate_eta2(const Cluster<T, 2, 2, int16_t> &cl) {
    Eta2<T> eta{};

    if ((cl.data[2] + cl.data[3]) != 0)
        eta.x = static_cast<double>(cl.data[2]) /
                (cl.data[2] + cl.data[3]); // between (0,1) the closer to zero
                                           // left value probably larger
    if ((cl.data[1] + cl.data[3]) != 0)
        eta.y = static_cast<double>(cl.data[1]) /
                (cl.data[1] + cl.data[3]); // between (0,1) the closer to zero
                                           // bottom value probably larger
    eta.sum = cl.sum();

    return eta;
}

template <typename T>
Eta2<T> calculate_full_eta2(const Cluster<T, 2, 2, int16_t> &cl) {
    Eta2<T> eta{};

    eta.sum = cl.sum();

    if (eta.sum != 0) {
        eta.x = static_cast<double>(cl.data[0] + cl.data[2]) /
                static_cast<double>(eta.sum); // between (0,1) the closer to
                                              // zero left value probably larger

        eta.y =
            static_cast<double>(cl.data[0] + cl.data[1]) /
            static_cast<double>(eta.sum); // between (0,1) the closer to zero
                                          // bottom value probably larger
    }
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

/**
 * @brief calculates cross Eta3 for 3x3 cluster
 * cross Eta3 calculates the eta by taking into account only the cross pixels
 * {top, bottom, left, right, center}
 */
template <typename T, typename CoordType = uint16_t>
Eta2<T> calculate_cross_eta3(const Cluster<T, 3, 3, CoordType> &cl) {

    Eta2<T> eta{};

    T photon_energy = cl.sum();

    eta.sum = photon_energy;

    if ((cl.data[3] + cl.data[4] + cl.data[5]) != 0)

        eta.x =
            static_cast<double>(-cl.data[3] + cl.data[3 + 2]) /

            static_cast<double>(cl.data[3] + cl.data[4] + cl.data[5]); // (-1,1)

    if ((cl.data[1] + cl.data[4] + cl.data[7]) != 0)

        eta.y = static_cast<double>(-cl.data[1] + cl.data[2 * 3 + 1]) /

                static_cast<double>(cl.data[1] + cl.data[4] + cl.data[7]);

    return eta;
}

/**
 * @brief calculates Eta3 for 3x3 cluster
 * It calculates the eta by taking into account all pixels in the 3x3 cluster
 */
template <typename T, typename CoordType = uint16_t>
Eta2<T> calculate_eta3(const Cluster<T, 3, 3, CoordType> &cl) {

    Eta2<T> eta{};

    T photon_energy = cl.sum();

    eta.sum = photon_energy;

    if (photon_energy != 0) {
        std::array<T, 2> column_sums{cl.data[0] + cl.data[3] + cl.data[6],
                                     cl.data[2] + cl.data[5] + cl.data[8]};

        eta.x = static_cast<double>(-column_sums[0] + column_sums[1]) /
                static_cast<double>(photon_energy);

        std::array<T, 2> row_sums{cl.data[0] + cl.data[1] + cl.data[2],
                                  cl.data[6] + cl.data[7] + cl.data[8]};

        eta.y = static_cast<double>(-row_sums[0] + row_sums[1]) /
                static_cast<double>(photon_energy);
    }

    return eta;
}

} // namespace aare