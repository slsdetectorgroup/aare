#pragma once
#include <cstddef>
#include <cstdint>
#include <numeric>
#include <vector>

#include <fmt/core.h>

namespace aare {

/**
 * @brief ClusterVector is a container for clusters of various sizes. It uses a 
 * contiguous memory buffer to store the clusters. 
 * @note push_back can invalidate pointers to elements in the container
 * @tparam T data type of the pixels in the cluster
 * @tparam CoordType data type of the x and y coordinates of the cluster (normally int16_t)
 */
template <typename T, typename CoordType=int16_t> class ClusterVector {
    using value_type = T;
    size_t m_cluster_size_x;
    size_t m_cluster_size_y;
    std::byte *m_data{};
    size_t m_size{0};
    size_t m_capacity;
    uint64_t m_frame_number{0}; //TODO! Check frame number size and type
    /*
    Format string used in the python bindings to create a numpy
    array from the buffer
    = - native byte order
    h - short
    d - double
    i - int
    */
    constexpr static char m_fmt_base[] = "=h:x:\nh:y:\n({},{}){}:data:" ;

  public:
    /**
     * @brief Construct a new ClusterVector object
     * @param cluster_size_x size of the cluster in x direction
     * @param cluster_size_y size of the cluster in y direction
     * @param capacity initial capacity of the buffer in number of clusters
     */
    ClusterVector(size_t cluster_size_x = 3, size_t cluster_size_y = 3,
                  size_t capacity = 1024, uint64_t frame_number = 0)
        : m_cluster_size_x(cluster_size_x), m_cluster_size_y(cluster_size_y),
          m_capacity(capacity), m_frame_number(frame_number) {
        allocate_buffer(capacity);
    }
    ~ClusterVector() {
        delete[] m_data;
    }

   
    //Move constructor
    ClusterVector(ClusterVector &&other) noexcept
        : m_cluster_size_x(other.m_cluster_size_x),
          m_cluster_size_y(other.m_cluster_size_y), m_data(other.m_data),
          m_size(other.m_size), m_capacity(other.m_capacity), m_frame_number(other.m_frame_number) {
        other.m_data = nullptr;
        other.m_size = 0;
        other.m_capacity = 0;
    }

    //Move assignment operator
    ClusterVector& operator=(ClusterVector &&other) noexcept {
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
     * @note If capacity is less than the current capacity, the function does nothing. 
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
     * @warning The data pointer must point to a buffer of size cluster_size_x * cluster_size_y * sizeof(T)
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
    ClusterVector& operator+=(const ClusterVector& other){
        if (m_size + other.m_size > m_capacity) {
            allocate_buffer(m_capacity + other.m_size);
        }
        std::copy(other.m_data, other.m_data + other.m_size * element_offset(), m_data + m_size * element_offset());
        m_size += other.m_size;
        return *this;
    }

    /**
     * @brief Sum the pixels in each cluster
     * @return std::vector<T> vector of sums for each cluster
     */
    std::vector<T> sum() {
        std::vector<T> sums(m_size);
        const size_t stride = element_offset();
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

    size_t size() const { return m_size; }
    size_t capacity() const { return m_capacity; }
    
    /**
     * @brief Return the offset in bytes for a single cluster
     */
    size_t element_offset() const {
        return 2*sizeof(CoordType)  +
               m_cluster_size_x * m_cluster_size_y * sizeof(T);
    }

    /**
     * @brief Return the size in bytes of a single cluster
     */
    size_t item_size() const { return element_offset(); }

    /**
     * @brief Return the offset in bytes for the i-th cluster
     */
    size_t element_offset(size_t i) const { return element_offset() * i; }

    /**
     * @brief Return a pointer to the i-th cluster
     */
    std::byte *element_ptr(size_t i) { return m_data + element_offset(i); }
     const std::byte *  element_ptr(size_t i) const { return m_data + element_offset(i); }

    size_t cluster_size_x() const { return m_cluster_size_x; }
    size_t cluster_size_y() const { return m_cluster_size_y; }

    std::byte *data() { return m_data; }
    std::byte const *data() const { return m_data; }

    template<typename V>
    V& at(size_t i) {
        return *reinterpret_cast<V*>(element_ptr(i));
    }

    const std::string_view fmt_base() const {
        //TODO! how do we match on coord_t?
        return m_fmt_base;
    }

    uint64_t frame_number() const { return m_frame_number; }
    void set_frame_number(uint64_t frame_number) { m_frame_number = frame_number; }
    void resize(size_t new_size) {
        if (new_size > m_capacity) {
            allocate_buffer(new_size);
        }
        m_size = new_size;
    }

  private:
    void allocate_buffer(size_t new_capacity) {
        size_t num_bytes = element_offset() * new_capacity;
        std::byte *new_data = new std::byte[num_bytes]{};
        std::copy(m_data, m_data + element_offset() * m_size, new_data);
        delete[] m_data;
        m_data = new_data;
        m_capacity = new_capacity;
    }
};

} // namespace aare