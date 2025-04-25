#pragma once
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
template <typename T, typename CoordType = int16_t> class ClusterVector {
    using value_type = T;
    size_t m_cluster_size_x;
    size_t m_cluster_size_y;
    std::byte *m_data{};
    size_t m_size{0};
    size_t m_capacity;
    uint64_t m_frame_number{0}; // TODO! Check frame number size and type
    /*
    Format string used in the python bindings to create a numpy
    array from the buffer
    = - native byte order
    h - short
    d - double
    i - int
    */
    constexpr static char m_fmt_base[] = "=h:x:\nh:y:\n({},{}){}:data:";

  public:
    /**
     * @brief Construct a new ClusterVector object
     * @param cluster_size_x size of the cluster in x direction
     * @param cluster_size_y size of the cluster in y direction
     * @param capacity initial capacity of the buffer in number of clusters
     * @param frame_number frame number of the clusters. Default is 0, which is
     * also used to indicate that the clusters come from many frames
     */
    ClusterVector(size_t cluster_size_x = 3, size_t cluster_size_y = 3,
                  size_t capacity = 1024, uint64_t frame_number = 0)
        : m_cluster_size_x(cluster_size_x), m_cluster_size_y(cluster_size_y),
          m_capacity(capacity), m_frame_number(frame_number) {
        allocate_buffer(capacity);
    }

    ~ClusterVector() { delete[] m_data; }

    // Move constructor
    ClusterVector(ClusterVector &&other) noexcept
        : m_cluster_size_x(other.m_cluster_size_x),
          m_cluster_size_y(other.m_cluster_size_y), m_data(other.m_data),
          m_size(other.m_size), m_capacity(other.m_capacity),
          m_frame_number(other.m_frame_number) {
        other.m_data = nullptr;
        other.m_size = 0;
        other.m_capacity = 0;
    }

    // Move assignment operator
    ClusterVector &operator=(ClusterVector &&other) noexcept {
        if (this != &other) {
            delete[] m_data;
            m_cluster_size_x = other.m_cluster_size_x;
            m_cluster_size_y = other.m_cluster_size_y;
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
     * @param x x-coordinate of the cluster
     * @param y y-coordinate of the cluster
     * @param data pointer to the data of the cluster
     * @warning The data pointer must point to a buffer of size cluster_size_x *
     * cluster_size_y * sizeof(T)
     */
    void push_back(CoordType x, CoordType y, const std::byte *data) {
        if (m_size == m_capacity) {
            allocate_buffer(m_capacity * 2);
        }
        std::byte *ptr = element_ptr(m_size);
        *reinterpret_cast<CoordType *>(ptr) = x;
        ptr += sizeof(CoordType);
        *reinterpret_cast<CoordType *>(ptr) = y;
        ptr += sizeof(CoordType);

        std::copy(data, data + m_cluster_size_x * m_cluster_size_y * sizeof(T),
                  ptr);
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
    std::vector<T> sum() {
        std::vector<T> sums(m_size);
        const size_t stride = item_size();
        const size_t n_pixels = m_cluster_size_x * m_cluster_size_y;
        std::byte *ptr = m_data + 2 * sizeof(CoordType); // skip x and y

        for (size_t i = 0; i < m_size; i++) {
            sums[i] =
                std::accumulate(reinterpret_cast<T *>(ptr),
                                reinterpret_cast<T *>(ptr) + n_pixels, T{});
            ptr += stride;
        }
        return sums;
    }

    /**
     * @brief Return the maximum sum of the 2x2 subclusters in each cluster
     * @return std::vector<T> vector of sums for each cluster
     * @throws std::runtime_error if the cluster size is not 3x3
     * @warning Only 3x3 clusters are supported for the 2x2 sum.
     */
    std::vector<T> sum_2x2() {
        std::vector<T> sums(m_size);
        const size_t stride = item_size();

        if (m_cluster_size_x != 3 || m_cluster_size_y != 3) {
            throw std::runtime_error(
                "Only 3x3 clusters are supported for the 2x2 sum.");
        }
        std::byte *ptr = m_data + 2 * sizeof(CoordType); // skip x and y

        for (size_t i = 0; i < m_size; i++) {
            std::array<T, 4> total;
            auto T_ptr = reinterpret_cast<T *>(ptr);
            total[0] = T_ptr[0] + T_ptr[1] + T_ptr[3] + T_ptr[4];
            total[1] = T_ptr[1] + T_ptr[2] + T_ptr[4] + T_ptr[5];
            total[2] = T_ptr[3] + T_ptr[4] + T_ptr[6] + T_ptr[7];
            total[3] = T_ptr[4] + T_ptr[5] + T_ptr[7] + T_ptr[8];

            sums[i] = *std::max_element(total.begin(), total.end());
            ptr += stride;
        }

        return sums;
    }

    /**
     * @brief Return the number of clusters in the vector
     */
    size_t size() const { return m_size; }

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
        return 2 * sizeof(CoordType) +
               m_cluster_size_x * m_cluster_size_y * sizeof(T);
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

    size_t cluster_size_x() const { return m_cluster_size_x; }
    size_t cluster_size_y() const { return m_cluster_size_y; }

    std::byte *data() { return m_data; }
    std::byte const *data() const { return m_data; }

    /**
     * @brief Return a reference to the i-th cluster casted to type V
     * @tparam V type of the cluster
     */
    template <typename V> V &at(size_t i) {
        return *reinterpret_cast<V *>(element_ptr(i));
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

    void apply_gain_map(const NDView<double> gain_map){
        //in principle we need to know the size of the image for this lookup
        //TODO! check orientations
        std::array<int64_t, 9> xcorr = {-1, 0, 1, -1, 0, 1, -1, 0, 1};
        std::array<int64_t, 9> ycorr = {-1, -1, -1, 0, 0, 0, 1, 1, 1};
        for (size_t i=0; i<m_size; i++){
            auto& cl = at<Cluster3x3>(i);

            if (cl.x > 0 && cl.y > 0 && cl.x < gain_map.shape(1)-1 && cl.y < gain_map.shape(0)-1){
                for (size_t j=0; j<9; j++){
                    size_t x = cl.x + xcorr[j];
                    size_t y = cl.y + ycorr[j];
                    cl.data[j] = static_cast<T>(cl.data[j] * gain_map(y, x));
                }
            }else{
                memset(cl.data, 0, 9*sizeof(T)); //clear edge clusters
            }

            
        }
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

} // namespace aare