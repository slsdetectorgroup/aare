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

    explicit InvertedGainMap(const NDView<double, 2> gain_map) {
        m_gain_map = NDArray<double, 2>(gain_map);
        for (auto &item : m_gain_map) {
            item = 1.0 / item;
        }
    }

    template <typename ClusterType,
              typename = std::enable_if_t<is_cluster_v<ClusterType>>>
    void apply_gain_map(ClusterVector<ClusterType> &clustervec) {
        using T = typename ClusterVector<ClusterType>::value_type;

        constexpr size_t cluster_size_x = ClusterType::cluster_size_x;
        constexpr size_t cluster_size_y = ClusterType::cluster_size_y;
        constexpr ssize_t left = cluster_size_x / 2;
        constexpr ssize_t right = cluster_size_x - left - 1;
        constexpr ssize_t top = cluster_size_y / 2;
        constexpr ssize_t bottom = cluster_size_y - top - 1;

        for (size_t i = 0; i < clustervec.size(); i++) {
            auto &cl = clustervec[i];
            const auto center_x = static_cast<ssize_t>(cl.x);
            const auto center_y = static_cast<ssize_t>(cl.y);

            if (center_x >= left && center_y >= top &&
                center_x < m_gain_map.shape(1) - right &&
                center_y < m_gain_map.shape(0) - bottom) {
                for (size_t j = 0; j < cluster_size_x * cluster_size_y; j++) {
                    const auto x = center_x +
                                   static_cast<ssize_t>(j % cluster_size_x) -
                                   left;
                    const auto y = center_y +
                                   static_cast<ssize_t>(j / cluster_size_x) -
                                   top;
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
