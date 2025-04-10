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
 * @brief ClusterVector is a container for clusters of various sizes. It uses a
 * contiguous memory buffer to store the clusters. It is templated on the data
 * type and the coordinate type of the clusters.
 * @note push_back can invalidate pointers to elements in the container
 * @warning ClusterVector is currently move only to catch unintended copies, but
 * this might change since there are probably use cases where copying is needed.
 * @tparam T data type of the pixels in the cluster
 * @tparam CoordType data type of the x and y coordinates of the cluster
 * (normally int16_t)
 */
#if 0
template <typename T, uint8_t ClusterSizeX, uint8_t ClusterSizeY,
          typename CoordType>
class ClusterVector<Cluster<T, ClusterSizeX, ClusterSizeY, CoordType>> {

    std::byte *m_data{};
    size_t m_size{0};
    size_t m_capacity;
    uint64_t m_frame_number{0}; // TODO! Check frame number size and type
    /** 
    Format string used in the python bindings to create a numpy
    array from the buffer
    = - native byte order
    h - short
    d - double
    i - int
    */
    constexpr static char m_fmt_base[] = "=h:x:\nh:y:\n({},{}){}:data:";

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
        : m_capacity(capacity), m_frame_number(frame_number) {
        allocate_buffer(m_capacity);
    }

    ~ClusterVector() { delete[] m_data; }

    // Move constructor
    ClusterVector(ClusterVector &&other) noexcept
        : m_data(other.m_data), m_size(other.m_size),
          m_capacity(other.m_capacity), m_frame_number(other.m_frame_number) {
        other.m_data = nullptr;
        other.m_size = 0;
        other.m_capacity = 0;
    }

    // Move assignment operator
    ClusterVector &operator=(ClusterVector &&other) noexcept {
        if (this != &other) {
            delete[] m_data;
            m_data = other.m_data;
            m_size = other.m_size;
            m_capacity = other.m_capacity;
            m_frame_number = other.m_frame_number;
            other.m_data = nullptr;
            other.m_size = 0;
            other.m_capacity = 0;
            other.m_frame_number = 0;
        }
        return *this;
    }

    /**
     * @brief Reserve space for at least capacity clusters
     * @param capacity number of clusters to reserve space for
     * @note If capacity is less than the current capacity, the function does
     * nothing.
     */
    void reserve(size_t capacity) {
        if (capacity > m_capacity) {
            allocate_buffer(capacity);
        }
    }

    /**
     * @brief Add a cluster to the vector
     */
    void push_back(const ClusterType &cluster) {
        if (m_size == m_capacity) {
            allocate_buffer(m_capacity * 2);
        }
        std::byte *ptr = element_ptr(m_size);
        *reinterpret_cast<CoordType *>(ptr) = cluster.x;
        ptr += sizeof(CoordType);
        *reinterpret_cast<CoordType *>(ptr) = cluster.y;
        ptr += sizeof(CoordType);

        std::memcpy(ptr, cluster.data, ClusterSizeX * ClusterSizeY * sizeof(T));

        m_size++;
    }

    ClusterVector &operator+=(const ClusterVector &other) {
        if (m_size + other.m_size > m_capacity) {
            allocate_buffer(m_capacity + other.m_size);
        }
        std::copy(other.m_data, other.m_data + other.m_size * item_size(),
                  m_data + m_size * item_size());
        m_size += other.m_size;
        return *this;
    }

    /**
     * @brief Sum the pixels in each cluster
     * @return std::vector<T> vector of sums for each cluster
     */
    /*
    std::vector<T> sum() {
        std::vector<T> sums(m_size);
        const size_t stride = item_size();
        const size_t n_pixels = ClusterSizeX * ClusterSizeY;
        std::byte *ptr = m_data + 2 * sizeof(CoordType); // skip x and y

        for (size_t i = 0; i < m_size; i++) {
            sums[i] =
                std::accumulate(reinterpret_cast<T *>(ptr),
                                reinterpret_cast<T *>(ptr) + n_pixels, T{});
            ptr += stride;
        }
        return sums;
    }
    */

    /**
     * @brief Sum the pixels in the 2x2 subcluster with the biggest pixel sum in
     * each cluster
     * @return std::vector<T> vector of sums for each cluster
     */ //TODO if underlying container is a vector use std::for_each
    /*
    std::vector<T> sum_2x2() {
        std::vector<T> sums_2x2(m_size);

        for (size_t i = 0; i < m_size; i++) {
            sums_2x2[i] = at(i).max_sum_2x2;
        }
        return sums_2x2;
    }
    */

    /**
     * @brief Return the number of clusters in the vector
     */
    size_t size() const { return m_size; }

    uint8_t cluster_size_x() const { return ClusterSizeX; }

    uint8_t cluster_size_y() const { return ClusterSizeY; }

    /**
     * @brief Return the capacity of the buffer in number of clusters. This is
     * the number of clusters that can be stored in the current buffer without
     * reallocation.
     */
    size_t capacity() const { return m_capacity; }

    /**
     * @brief Return the size in bytes of a single cluster
     */
    size_t item_size() const {
        return 2 * sizeof(CoordType) + ClusterSizeX * ClusterSizeY * sizeof(T);
    }

    /**
     * @brief Return the offset in bytes for the i-th cluster
     */
    size_t element_offset(size_t i) const { return item_size() * i; }

    /**
     * @brief Return a pointer to the i-th cluster
     */
    std::byte *element_ptr(size_t i) { return m_data + element_offset(i); }

    /**
     * @brief Return a pointer to the i-th cluster
     */
    const std::byte *element_ptr(size_t i) const {
        return m_data + element_offset(i);
    }

    std::byte *data() { return m_data; }
    std::byte const *data() const { return m_data; }

    /**
     * @brief Return a reference to the i-th cluster casted to type V
     * @tparam V type of the cluster
     */
    ClusterType &at(size_t i) {
        return *reinterpret_cast<ClusterType *>(element_ptr(i));
    }

    const ClusterType &at(size_t i) const {
        return *reinterpret_cast<const ClusterType *>(element_ptr(i));
    }

    template <typename V> const V &at(size_t i) const {
        return *reinterpret_cast<const V *>(element_ptr(i));
    }

    const std::string_view fmt_base() const {
        // TODO! how do we match on coord_t?
        return m_fmt_base;
    }

    /** 
     * @brief Return the frame number of the clusters. 0 is used to indicate
     * that the clusters come from many frames
     */
    uint64_t frame_number() const { return m_frame_number; }

    void set_frame_number(uint64_t frame_number) {
        m_frame_number = frame_number;
    }

    /** 
     * @brief Resize the vector to contain new_size clusters. If new_size is
     * greater than the current capacity, a new buffer is allocated. If the size
     * is smaller no memory is freed, size is just updated.
     * @param new_size new size of the vector
     * @warning The additional clusters are not initialized
     */
    void resize(size_t new_size) {
        // TODO! Should we initialize the new clusters?
        if (new_size > m_capacity) {
            allocate_buffer(new_size);
        }
        m_size = new_size;
    }

  private:
    void allocate_buffer(size_t new_capacity) {
        size_t num_bytes = item_size() * new_capacity;
        std::byte *new_data = new std::byte[num_bytes]{};
        std::copy(m_data, m_data + item_size() * m_size, new_data);
        delete[] m_data;
        m_data = new_data;
        m_capacity = new_capacity;
    }
};
#endif

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
class ClusterVector<Cluster<T, ClusterSizeX, ClusterSizeY, CoordType>> {

    std::vector<Cluster<T, ClusterSizeX, ClusterSizeY, CoordType>> m_data{};
    uint64_t m_frame_number{0}; // TODO! Check frame number size and type

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
        m_data.resize(capacity);
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

    uint8_t cluster_size_x() const { return ClusterSizeX; }

    uint8_t cluster_size_y() const { return ClusterSizeY; }

    /**
     * @brief Return the capacity of the buffer in number of clusters. This is
     * the number of clusters that can be stored in the current buffer without
     * reallocation.
     */
    size_t capacity() const { return m_data.capacity(); }

    const auto begin() const { return m_data.begin(); }

    const auto end() const { return m_data.end(); }

    /**
     * @brief Return the size in bytes of a single cluster
     */
    size_t item_size() const {
        return 2 * sizeof(CoordType) + ClusterSizeX * ClusterSizeY * sizeof(T);
    }

    ClusterType *data() { return m_data.data(); }
    ClusterType const *data() const { return m_data.data(); }

    /**
     * @brief Return a reference to the i-th cluster casted to type V
     * @tparam V type of the cluster
     */
    ClusterType &at(size_t i) { return m_data[i]; }

    const ClusterType &at(size_t i) const { return m_data[i]; }

    /**
     * @brief Return the frame number of the clusters. 0 is used to indicate
     * that the clusters come from many frames
     */
    uint64_t frame_number() const { return m_frame_number; }

    void set_frame_number(uint64_t frame_number) {
        m_frame_number = frame_number;
    }
};

} // namespace aare