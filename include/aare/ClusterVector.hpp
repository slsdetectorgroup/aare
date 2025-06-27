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
 * @brief ClusterVector is a container for clusters of various sizes. It
 * uses a contiguous memory buffer to store the clusters. It is templated on
 * the data type and the coordinate type of the clusters.
 * @note push_back can invalidate pointers to elements in the container
 * @warning ClusterVector is currently move only to catch unintended copies,
 * but this might change since there are probably use cases where copying is
 * needed.
 * @tparam T data type of the pixels in the cluster
 * @tparam CoordType data type of the x and y coordinates of the cluster
 * (normally int16_t)
 */
template <typename T, uint8_t ClusterSizeX, uint8_t ClusterSizeY,
          typename CoordType>
class ClusterVector<Cluster<T, ClusterSizeX, ClusterSizeY, CoordType>> 
{

    std::vector<Cluster<T, ClusterSizeX, ClusterSizeY, CoordType>> m_data{};
    int32_t m_frame_number{0}; // TODO! Check frame number size and type

  public:
    using value_type = T;
    using ClusterType = Cluster<T, ClusterSizeX, ClusterSizeY, CoordType>;

    /**
     * @brief Construct a new ClusterVector object
     * @param capacity initial capacity of the buffer in number of clusters
     * @param frame_number frame number of the clusters. Default is 0, which is
     * also used to indicate that the clusters come from many frames
     */
    ClusterVector(size_t capacity = 1024, uint64_t frame_number = 0)
        : m_frame_number(frame_number) {
        m_data.reserve(capacity);
    }

    // Move constructor
    ClusterVector(ClusterVector &&other) noexcept
        : m_data(other.m_data), m_frame_number(other.m_frame_number) {
        other.m_data.clear();
    }

    // Move assignment operator
    ClusterVector &operator=(ClusterVector &&other) noexcept {
        if (this != &other) {
            m_data = other.m_data;
            m_frame_number = other.m_frame_number;
            other.m_data.clear();
            other.m_frame_number = 0;
        }
        return *this;
    }

    /**
     * @brief Sum the pixels in each cluster
     * @return std::vector<T> vector of sums for each cluster
     */
    std::vector<T> sum() {
        std::vector<T> sums(m_data.size());

        std::transform(
            m_data.begin(), m_data.end(), sums.begin(),
            [](const ClusterType &cluster) { return cluster.sum(); });

        return sums;
    }

    /**
     * @brief Sum the pixels in the 2x2 subcluster with the biggest pixel sum in
     * each cluster
     * @return std::vector<T> vector of sums for each cluster
     */
    std::vector<T> sum_2x2() {
        std::vector<T> sums_2x2(m_data.size());

        std::transform(m_data.begin(), m_data.end(), sums_2x2.begin(),
                       [](const ClusterType &cluster) {
                           return cluster.max_sum_2x2().first;
                       });

        return sums_2x2;
    }

    /**
     * @brief Reserve space for at least capacity clusters
     * @param capacity number of clusters to reserve space for
     * @note If capacity is less than the current capacity, the function does
     * nothing.
     */
    void reserve(size_t capacity) { m_data.reserve(capacity); }

    void resize(size_t size) { m_data.resize(size); }

    void push_back(const ClusterType &cluster) { m_data.push_back(cluster); }

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

    uint8_t cluster_size_x() const { return ClusterSizeX; }

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
     * @brief Return the size in bytes of a single cluster
     */
    size_t item_size() const {
        return sizeof(ClusterType); // 2 * sizeof(CoordType) + ClusterSizeX *
                                    // ClusterSizeY * sizeof(T);
    }

    ClusterType *data() { return m_data.data(); }
    ClusterType const *data() const { return m_data.data(); }

    /**
     * @brief Return a reference to the i-th cluster casted to type V
     * @tparam V type of the cluster
     */
    ClusterType &operator[](size_t i) { return m_data[i]; }

    const ClusterType &operator[](size_t i) const { return m_data[i]; }

    /**
     * @brief Return the frame number of the clusters. 0 is used to indicate
     * that the clusters come from many frames
     */
    int32_t frame_number() const { return m_frame_number; }

    void set_frame_number(int32_t frame_number) {
        m_frame_number = frame_number;
    }
};

} // namespace aare