// SPDX-License-Identifier: MPL-2.0
#pragma once
#include "aare/Cluster.hpp" //TODO maybe store in seperate file !!!
#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <numeric>
#include <vector>

#include <fmt/core.h>

#include "aare/Cluster.hpp"
#include "aare/NDView.hpp"

namespace aare {

template <typename ClusterType,
          typename = std::enable_if_t<is_cluster_v<ClusterType>>>
class ClusterVector; // Forward declaration

/**
 * @brief Move-only container that stores fixed-size clusters contiguously.
 *
 * The pixel type, cluster dimensions, and coordinate type are determined by
 * the Cluster specialization supplied as the template argument.
 *
 * @note push_back, reserve, and resize can invalidate pointers, references, and
 * iterators to elements in the container.
 * @warning ClusterVector is currently move only to catch unintended copies,
 * but this might change since there are probably use cases where copying is
 * needed.
 * @tparam T data type of the pixels in the cluster
 * @tparam ClusterSizeX cluster size in the x dimension
 * @tparam ClusterSizeY cluster size in the y dimension
 * @tparam CoordType data type of the x and y coordinates of the cluster
 * (normally uint16_t)
 */
template <typename T, uint8_t ClusterSizeX, uint8_t ClusterSizeY,
          typename CoordType>
class ClusterVector<Cluster<T, ClusterSizeX, ClusterSizeY, CoordType>> {

    std::vector<Cluster<T, ClusterSizeX, ClusterSizeY, CoordType>> m_data{};
    int32_t m_frame_number{0}; // TODO! Check frame number size and type

  public:
    using value_type = T;
    using ClusterType = Cluster<T, ClusterSizeX, ClusterSizeY, CoordType>;

    /**
     * @brief Construct a new ClusterVector object
     * @param capacity minimum initial capacity in number of clusters
     * @param frame_number frame number of the clusters. Default is 0, which is
     * also used to indicate that the clusters come from many frames
     */
    ClusterVector(size_t capacity = 1024, int32_t frame_number = 0)
        : m_frame_number(frame_number) {
        m_data.reserve(capacity);
    }

    ClusterVector(ClusterVector &&other) noexcept = default;
    ClusterVector &operator=(ClusterVector &&other) noexcept = default;

    /**
     * @brief Return a filtered copy selected by a one-dimensional Boolean mask.
     * @param mask mask with one element for every cluster
     * @return ClusterVector containing the selected clusters in their original
     * order and with the original frame number
     * @throws std::runtime_error if the mask length differs from size()
     */
    ClusterVector operator()(NDView<bool, 1> mask) {
        if (static_cast<size_t>(mask.size()) != m_data.size()) {
            throw std::runtime_error(
                LOCATION + "Mask size does not match number of clusters");
        }
        if (m_data.empty()) {
            return ClusterVector(0, frame_number());
        }
        const auto selected =
            static_cast<size_t>(std::count(mask.begin(), mask.end(), true));
        ClusterVector result(selected, frame_number());
        for (size_t i = 0; i < m_data.size(); ++i) {
            if (mask(i)) {
                result.push_back(m_data[i]);
            }
        }
        return result;
    }

    /**
     * @brief Sum the pixels in each cluster.
     * @return One sum for every cluster, in container order
     */
    std::vector<T> sum() {
        std::vector<T> sums(m_data.size());

        std::transform(
            m_data.begin(), m_data.end(), sums.begin(),
            [](const ClusterType &cluster) { return cluster.sum(); });

        return sums;
    }

    /**
     * @brief Find the highest-sum center-adjacent 2x2 subcluster in each
     * cluster.
     * @return One sum and corner-index pair for every cluster, in container
     * order
     */
    std::vector<Sum_index_pair<T, corner>> sum_2x2() {
        std::vector<Sum_index_pair<T, corner>> sums_2x2(m_data.size());

        std::transform(
            m_data.begin(), m_data.end(), sums_2x2.begin(),
            [](const ClusterType &cluster) { return cluster.max_sum_2x2(); });

        return sums_2x2;
    }

    /**
     * @brief Reserve space for at least capacity clusters
     * @param capacity number of clusters to reserve space for
     * @note If capacity is less than the current capacity, the function does
     * nothing.
     */
    void reserve(size_t capacity) { m_data.reserve(capacity); }

    /**
     * @brief Change the number of stored clusters.
     * @param size new number of clusters
     * @note Growing the vector value-initializes new clusters and can
     * invalidate pointers, references, and iterators.
     */
    void resize(size_t size) { m_data.resize(size); }

    /**
     * @brief Append a cluster to the vector.
     * @param cluster cluster to append
     * @note Reallocation invalidates pointers, references, iterators, and
     * zero-copy NumPy views of the storage.
     */
    void push_back(const ClusterType &cluster) { m_data.push_back(cluster); }

    /**
     * @brief Append all clusters from another vector.
     * @param other vector whose clusters are appended
     * @return Reference to this vector
     * @note The frame number of this vector is unchanged.
     * @warning other must not refer to this vector.
     */
    ClusterVector &operator+=(const ClusterVector &other) {
        m_data.insert(m_data.end(), other.begin(), other.end());

        return *this;
    }

    /**
     * @brief Return the number of clusters in the vector
     */
    size_t size() const { return m_data.size(); }

    /**
     * @brief Check if the vector is empty
     */
    bool empty() const { return m_data.empty(); }

    /** @brief Return the cluster size in the x dimension. */
    uint8_t cluster_size_x() const { return ClusterSizeX; }

    /** @brief Return the cluster size in the y dimension. */
    uint8_t cluster_size_y() const { return ClusterSizeY; }

    /**
     * @brief Return the capacity of the buffer in number of clusters. This is
     * the number of clusters that can be stored in the current buffer without
     * reallocation.
     */
    size_t capacity() const { return m_data.capacity(); }

    auto begin() const { return m_data.begin(); }

    auto end() const { return m_data.end(); }

    /**
     * @brief Return the size in bytes of one stored cluster, including padding.
     */
    size_t item_size() const {
        return sizeof(ClusterType); // 2 * sizeof(CoordType) + ClusterSizeX *
                                    // ClusterSizeY * sizeof(T);
    }

    ClusterType *data() { return m_data.data(); }
    ClusterType const *data() const { return m_data.data(); }

    /**
     * @brief Return a reference to the i-th cluster without bounds checking.
     * @param i zero-based cluster index
     */
    ClusterType &operator[](size_t i) { return m_data[i]; }

    const ClusterType &operator[](size_t i) const { return m_data[i]; }

    /**
     * @brief Return the frame number of the clusters. 0 is used to indicate
     * that the clusters come from many frames
     */
    int32_t frame_number() const { return m_frame_number; }

    /**
     * @brief Set the signed 32-bit frame number associated with the clusters.
     * @param frame_number frame number, or 0 for clusters from multiple frames
     */
    void set_frame_number(int32_t frame_number) {
        m_frame_number = frame_number;
    }
};

/**
 * @brief Reduce every cluster to its highest-sum center-adjacent 2x2 block.
 * @param cv ClusterVector containing clusters to reduce
 * @return ClusterVector of 2x2 clusters in the original order and with the
 * original frame number
 * @note Output cluster data is stored in row-major order. Coordinates are
 * preserved.
 */
template <typename T, uint8_t ClusterSizeX, uint8_t ClusterSizeY,
          typename CoordType>
ClusterVector<Cluster<T, 2, 2, CoordType>> reduce_to_2x2(
    const ClusterVector<Cluster<T, ClusterSizeX, ClusterSizeY, CoordType>>
        &cv) {
    ClusterVector<Cluster<T, 2, 2, CoordType>> result(cv.size(),
                                                      cv.frame_number());
    for (const auto &c : cv) {
        result.push_back(reduce_to_2x2(c));
    }
    return result;
}

/**
 * @brief Reduce every cluster to the 3x3 block around its center index.
 * @param cv ClusterVector containing clusters to reduce
 * @return ClusterVector of 3x3 clusters in the original order and with the
 * original frame number
 * @note Coordinates are preserved.
 */
template <typename T, uint8_t ClusterSizeX, uint8_t ClusterSizeY,
          typename CoordType>
ClusterVector<Cluster<T, 3, 3, CoordType>> reduce_to_3x3(
    const ClusterVector<Cluster<T, ClusterSizeX, ClusterSizeY, CoordType>>
        &cv) {
    ClusterVector<Cluster<T, 3, 3, CoordType>> result(cv.size(),
                                                      cv.frame_number());
    for (const auto &c : cv) {
        result.push_back(reduce_to_3x3(c));
    }
    return result;
}

} // namespace aare
