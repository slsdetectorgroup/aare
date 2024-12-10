#pragma once
#include <cstddef>
#include <cstdint>

template <typename T> class ClusterVector {
    int32_t m_cluster_size_x;
    int32_t m_cluster_size_y;
    std::byte *m_data{};
    size_t m_size{0};
    size_t m_capacity{10};

  public:
    ClusterVector(int32_t cluster_size_x, int32_t cluster_size_y)
        : m_cluster_size_x(cluster_size_x), m_cluster_size_y(cluster_size_y) {
        size_t num_bytes = element_offset() * m_capacity;
        m_data = new std::byte[num_bytes]{};
        // fmt::print("Allocating {} bytes\n", num_bytes);
    }

    // data better hold data of the right size!
    void push_back(int32_t x, int32_t y, const std::byte *data) {
        if (m_size == m_capacity) {
            m_capacity *= 2;
            std::byte *new_data =
                new std::byte[element_offset()*m_capacity]{};
            std::copy(m_data,
                      m_data + element_offset()*m_size,
                      new_data);
            delete[] m_data;
            m_data = new_data;
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

    std::vector<T> sum(){
        std::vector<T> sums(m_size);
        for (size_t i = 0; i < m_size; i++) {
            T sum = 0;
            std::byte *ptr = element_ptr(i) + 2 * sizeof(int32_t);
            for (size_t j = 0; j < m_cluster_size_x * m_cluster_size_y; j++) {
                sum += *reinterpret_cast<T *>(ptr);
                ptr += sizeof(T);
            }
            sums[i] = sum;
        }
        return sums;
    }

    size_t size() const { return m_size; }
    size_t element_offset() const {
        return sizeof(m_cluster_size_x) + sizeof(m_cluster_size_y) +
               m_cluster_size_x * m_cluster_size_y * sizeof(T);
    }
    size_t element_offset(size_t i) const {
        return element_offset() * i;
    }

    std::byte* element_ptr(size_t i) {
        return m_data + element_offset(i);
    }

    int16_t cluster_size_x() const { return m_cluster_size_x; }
    int16_t cluster_size_y() const { return m_cluster_size_y; }

    std::byte *data() { return m_data; }

    ~ClusterVector() { delete[] m_data; }
};