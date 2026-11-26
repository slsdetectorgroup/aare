// SPDX-License-Identifier: MPL-2.0
/************************************************
 * @file GainMap.hpp
 * @short function to apply gain map of image size to a vector of clusters -
 *note stored gainmap is inverted for efficient aaplication to images
 ***********************************************/

#pragma once
#include "aare/Cluster.hpp"
#include "aare/ClusterVector.hpp"
#include "aare/NDArray.hpp"
#include "aare/NDView.hpp"
#include <memory>

namespace aare {

class InvertedGainMap {

  public:
    explicit InvertedGainMap(const NDArray<double, 2> &gain_map)
        : m_gain_map(gain_map) {
        for (auto &item : m_gain_map) {
            item = 1.0 / item;
        }
    };

    explicit InvertedGainMap(const NDView<const double, 2> gain_map)
        : m_gain_map(gain_map) {
        for (auto &item : m_gain_map) {
            item = 1.0 / item;
        }
    }

    /**
     * @brief Whether every pixel of a cluster with the given center lies
     * within the gain map.
     * @param center_x x coordinate of the cluster (Cluster::x)
     * @param center_y y coordinate of the cluster (Cluster::y)
     */
    template <typename ClusterType,
              typename = std::enable_if_t<is_cluster_v<ClusterType>>>
    bool covers(const ssize_t center_x, const ssize_t center_y) const {
        constexpr ssize_t cluster_size_x = ClusterType::cluster_size_x;
        constexpr ssize_t cluster_size_y = ClusterType::cluster_size_y;
        constexpr ssize_t pixels_left_from_center = cluster_size_x / 2;
        constexpr ssize_t pixels_right_from_center =
            cluster_size_x - pixels_left_from_center - 1;
        constexpr ssize_t pixels_top_from_center = cluster_size_y / 2;
        constexpr ssize_t pixels_bottom_from_center =
            cluster_size_y - pixels_top_from_center - 1;

        return center_x >= pixels_left_from_center &&
               center_y >= pixels_top_from_center &&
               center_x < m_gain_map.shape(1) - pixels_right_from_center &&
               center_y < m_gain_map.shape(0) - pixels_bottom_from_center;
    }

    /// @brief Inverted gain (energy per ADU) of the pixel at row y, column x.
    double operator()(ssize_t y, ssize_t x) const { return m_gain_map(y, x); }

    template <typename ClusterType,
              typename = std::enable_if_t<is_cluster_v<ClusterType>>>
    void apply_gain_map(ClusterVector<ClusterType> &clustervec) {
        using T = typename ClusterVector<ClusterType>::value_type;

        constexpr size_t cluster_size_x = ClusterType::cluster_size_x;
        constexpr size_t cluster_size_y = ClusterType::cluster_size_y;
        constexpr ssize_t pixels_left_from_center = cluster_size_x / 2;
        constexpr ssize_t pixels_top_from_center = cluster_size_y / 2;

        for (size_t i = 0; i < clustervec.size(); i++) {
            auto &cl = clustervec[i];
            const auto center_x = static_cast<ssize_t>(cl.x);
            const auto center_y = static_cast<ssize_t>(cl.y);

            if (covers<ClusterType>(center_x, center_y)) {
                for (size_t j = 0; j < cluster_size_x * cluster_size_y; j++) {
                    const auto x = center_x +
                                   static_cast<ssize_t>(j % cluster_size_x) -
                                   pixels_left_from_center;
                    const auto y = center_y +
                                   static_cast<ssize_t>(j / cluster_size_x) -
                                   pixels_top_from_center;
                    cl.data[j] = static_cast<T>(
                        static_cast<double>(cl.data[j]) *
                        m_gain_map(
                            y, x)); // cast after conversion to keep precision
                }
            } else {
                // Clear clusters whose footprint extends beyond the gain map.
                cl.data.fill(0);
            }
        }
    }

  private:
    NDArray<double, 2> m_gain_map{};
};

} // end of namespace aare
