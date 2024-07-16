#pragma once
#include "aare/core/Dtype.hpp"
#include <array>
#include <cstdint>
#include <cstring>
#include <stdexcept>

namespace aare {

/*
 * TODO: Should be removed
 */
class Cluster {
  public:
    int cluster_sizeX;
    int cluster_sizeY;
    int16_t x;
    int16_t y;
    Dtype dt;

  private:
    std::byte *m_data;

  public:
    Cluster(int cluster_sizeX_, int cluster_sizeY_, Dtype dt_ = Dtype(typeid(int32_t)))
        : cluster_sizeX(cluster_sizeX_), cluster_sizeY(cluster_sizeY_), dt(dt_) {
        m_data = new std::byte[cluster_sizeX * cluster_sizeY * dt.bytes()]{};
    }
    Cluster() : Cluster(3, 3) {}
    Cluster(const Cluster &other) : Cluster(other.cluster_sizeX, other.cluster_sizeY, other.dt) {
        if (this == &other)
            return;
        x = other.x;
        y = other.y;
        std::memcpy(m_data, other.m_data, other.bytes());
    }
    Cluster &operator=(const Cluster &other) {
        if (this == &other)
            return *this;
        this->~Cluster();
        new (this) Cluster(other);
        return *this;
    }
    Cluster(Cluster &&other) noexcept
        : cluster_sizeX(other.cluster_sizeX), cluster_sizeY(other.cluster_sizeY), x(other.x),
          y(other.y), dt(other.dt), m_data(other.m_data) {
        other.m_data = nullptr;
        other.dt = Dtype(Dtype::TypeIndex::ERROR);
    }
    ~Cluster() { delete[] m_data; }
    template <typename T> T get(int idx) {
        (sizeof(T) == dt.bytes()) ? 0 : throw std::invalid_argument("[ERROR] Type size mismatch");
        return *reinterpret_cast<T *>(m_data + idx * dt.bytes());
    }
    template <typename T> auto set(int idx, T val) {
        (sizeof(T) == dt.bytes()) ? 0 : throw std::invalid_argument("[ERROR] Type size mismatch");
        return memcpy(m_data + idx * dt.bytes(), &val, (size_t)dt.bytes());
    }
    // auto x() const { return x; }
    // auto y() const { return y; }
    // auto x(int16_t x_) { return x = x_; }
    // auto y(int16_t y_) { return y = y_; }

    template <typename T> std::string to_string() const {
        (sizeof(T) == dt.bytes()) ? 0 : throw std::invalid_argument("[ERROR] Type size mismatch");
        std::string s = "x: " + std::to_string(x) + " y: " + std::to_string(y) + "\nm_data: [";
        for (int i = 0; i < cluster_sizeX * cluster_sizeY; i++) {
            s += std::to_string(*reinterpret_cast<T *>(m_data + i * dt.bytes())) + " ";
        }
        s += "]";
        return s;
    }
    /**
     * @brief size of the cluster in bytes when saved to a file
     */
    size_t size() const { return cluster_sizeX * cluster_sizeY; }
    size_t bytes() const { return cluster_sizeX * cluster_sizeY * dt.bytes(); }
    auto begin() const { return m_data; }
    auto end() const { return m_data + cluster_sizeX * cluster_sizeY * dt.bytes(); }
    std::byte *data() { return m_data; }
};

/*
 * new Cluster class
 *
 */

// class to hold the header of the old cluster format
namespace v3 {
struct ClusterHeader {
    int32_t frame_number;
    int32_t n_clusters;
    ClusterHeader() : frame_number(0), n_clusters(0) {}
    ClusterHeader(int32_t frame_number_, int32_t n_clusters_)
        : frame_number(frame_number_), n_clusters(n_clusters_) {}

    // interface functions (mandatory)
    void set(std::byte *data_) {
        // std::copy(data, data + sizeof(frame_number), &frame_number);
        // std::copy(data + sizeof(frame_number), data + 2 * sizeof(frame_number), &n_clusters);
        std::memcpy(&frame_number, data_, sizeof(frame_number));
        std::memcpy(&n_clusters, data_ + sizeof(frame_number), sizeof(n_clusters));
    }
    void get(std::byte *data_) {
        // std::copy(&frame_number, &frame_number + sizeof(frame_number), data);
        // std::copy(&n_clusters, &n_clusters + sizeof(n_clusters), data + sizeof(frame_number));
        std::memcpy(data_, &frame_number, sizeof(frame_number));
        std::memcpy(data_ + sizeof(frame_number), &n_clusters, sizeof(n_clusters));
    }
    int32_t data_count() const { return n_clusters; }
    // used to indicate that data can be read directly into the struct using data()
    // e.g. fread(header.data(), sizeof(header), 1, file);
    constexpr static bool has_data() { return true; }
    std::byte *data() { return reinterpret_cast<std::byte *>(this); }
    constexpr size_t size() { return sizeof(frame_number) + sizeof(n_clusters); }

    // interface functions (optional)
    std::string to_string() const {
        return "frame_number: " + std::to_string(frame_number) +
               " n_clusters: " + std::to_string(n_clusters);
    }
};

// class to hold the data of the old cluster format
struct ClusterData {
    int16_t m_x;
    int16_t m_y;
    std::array<int32_t, 9> m_data;

    ClusterData() : m_x(0), m_y(0), m_data({}) {}
    ClusterData(int16_t x_, int16_t y_, std::array<int32_t, 9> data_)
        : m_x(x_), m_y(y_), m_data(data_) {}
    void set(std::byte *data_) {
        std::memcpy(&m_x, data_, sizeof(m_x));
        std::memcpy(&m_y, data_ + sizeof(m_x), sizeof(m_y));
        std::memcpy(m_data.data(), data_ + 2 * sizeof(m_x), 9 * sizeof(int32_t));
    }
    void get(std::byte *data_) {
        std::memcpy(data_, &m_x, sizeof(m_x));
        std::memcpy(data_ + sizeof(m_x), &m_y, sizeof(m_y));
        std::memcpy(data_ + 2 * sizeof(m_x), m_data.data(), 9 * sizeof(int32_t));
    }
    constexpr static bool has_data() { return true; }
    std::byte *data() { return reinterpret_cast<std::byte *>(this); }
    constexpr size_t size() { return sizeof(m_x) + sizeof(m_x) + 9 * sizeof(int32_t); }

    std::string to_string() const {
        std::string s = "x: " + std::to_string(m_x) + " y: " + std::to_string(m_y) + "\ndata: [";
        for (auto &d : m_data) {
            s += std::to_string(d) + " ";
        }
        s += "]";
        return s;
    }
};

} // namespace v3

} // namespace aare