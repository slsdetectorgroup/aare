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
        // in principle we need to know the size of the image for this lookup
        size_t ClusterSizeX = clustervec.cluster_size_x();
        size_t ClusterSizeY = clustervec.cluster_size_y();

        using T = typename ClusterVector<ClusterType>::value_type;

        int64_t index_cluster_center_x = ClusterSizeX / 2;
        int64_t index_cluster_center_y = ClusterSizeY / 2;
        for (size_t i = 0; i < clustervec.size(); i++) {
            auto &cl = clustervec[i];

            if (cl.x > 0 && cl.y > 0 && cl.x < m_gain_map.shape(1) - 1 &&
                cl.y < m_gain_map.shape(0) - 1) {
                for (size_t j = 0; j < ClusterSizeX * ClusterSizeY; j++) {
                    size_t x = cl.x + j % ClusterSizeX - index_cluster_center_x;
                    size_t y = cl.y + j / ClusterSizeX - index_cluster_center_y;
                    cl.data[j] = static_cast<T>(
                        static_cast<double>(cl.data[j]) *
                        m_gain_map(
                            y, x)); // cast after conversion to keep precision
                }
            } else {
                // clear edge clusters
                cl.data.fill(0);
            }
        }
    }

  private:
    NDArray<double, 2> m_gain_map{};
};

} // end of namespace aare