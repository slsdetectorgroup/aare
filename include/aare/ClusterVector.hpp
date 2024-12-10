#pragma once
#include <cstddef>
#include <cstdint>
#include <numeric>

#include <fmt/core.h>

template <typename T> class ClusterVector {
    int32_t m_cluster_size_x;
    int32_t m_cluster_size_y;
    std::byte *m_data{};
    size_t m_size{0};
    size_t m_capacity;

  public:
    ClusterVector(int32_t cluster_size_x, int32_t cluster_size_y,
                  size_t capacity = 1024)
        : m_cluster_size_x(cluster_size_x), m_cluster_size_y(cluster_size_y),
          m_capacity(capacity) {
        allocate_buffer(capacity);
    }
    ~ClusterVector() {
        fmt::print("~ClusterVector - size: {}, capacity: {}\n", m_size,
                   m_capacity);
        delete[] m_data;
    }

    // ClusterVector(const ClusterVector & other){
    //     m_cluster_size_x = other.m_cluster_size_x;
    //     m_cluster_size_y = other.m_cluster_size_y;
    //     m_data = new std::byte[other.m_capacity];
    //     std::copy(other.m_data, other.m_data + other.m_capacity, m_data);
    //     m_size = other.m_size;
    //     m_capacity = other.m_capacity;
    // }

    ClusterVector(ClusterVector &&other) noexcept
        : m_cluster_size_x(other.m_cluster_size_x),
          m_cluster_size_y(other.m_cluster_size_y), m_data(other.m_data),
          m_size(other.m_size), m_capacity(other.m_capacity) {
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
            other.m_data = nullptr;
            other.m_size = 0;
            other.m_capacity = 0;
        }
        return *this;
    }

    void reserve(size_t capacity) {
        if (capacity > m_capacity) {
            allocate_buffer(capacity);
        }
    }

    // data better hold data of the right size!
    void push_back(int32_t x, int32_t y, const std::byte *data) {
        if (m_size == m_capacity) {
            allocate_buffer(m_capacity * 2);
        }
        std::byte *ptr = element_ptr(m_size);
        *reinterpret_cast<int32_t *>(ptr) = x;
        ptr += sizeof(int32_t);
        *reinterpret_cast<int32_t *>(ptr) = y;
        ptr += sizeof(int32_t);

        std::copy(data, data + m_cluster_size_x * m_cluster_size_y * sizeof(T),
                  ptr);
        m_size++;
    }

    std::vector<T> sum() {
        std::vector<T> sums(m_size);
        const size_t stride = element_offset();
        const size_t n_pixels = m_cluster_size_x * m_cluster_size_y;
        std::byte *ptr = m_data + 2 * sizeof(int32_t); // skip x and y

        for (size_t i = 0; i < m_size; i++) {
            sums[i] =
                std::accumulate(reinterpret_cast<T *>(ptr),
                                reinterpret_cast<T *>(ptr) + n_pixels, T{});
            ptr += stride;
        }
        return sums;
    }

    size_t size() const { return m_size; }
    size_t element_offset() const {
        return sizeof(m_cluster_size_x) + sizeof(m_cluster_size_y) +
               m_cluster_size_x * m_cluster_size_y * sizeof(T);
    }
    size_t element_offset(size_t i) const { return element_offset() * i; }

    std::byte *element_ptr(size_t i) { return m_data + element_offset(i); }

    int16_t cluster_size_x() const { return m_cluster_size_x; }
    int16_t cluster_size_y() const { return m_cluster_size_y; }

    std::byte *data() { return m_data; }

  private:
    void allocate_buffer(size_t new_capacity) {
        size_t num_bytes = element_offset() * new_capacity;
        fmt::print(
            "ClusterVector allocating {} elements for a total of {} bytes\n",
            new_capacity, num_bytes);
        std::byte *new_data = new std::byte[num_bytes]{};
        std::copy(m_data, m_data + element_offset() * m_size, new_data);
        delete[] m_data;
        m_data = new_data;
        m_capacity = new_capacity;
    }
};