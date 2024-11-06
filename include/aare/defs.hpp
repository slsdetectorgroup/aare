#pragma once

#include "aare/Dtype.hpp"
// #include "aare/utils/logger.hpp"

#include <array>
#include <stdexcept>

#include <cassert>
#include <cstdint>
#include <cstring>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

/**
 * @brief LOCATION macro to get the current location in the code
 */
#define LOCATION                                                               \
    std::string(__FILE__) + std::string(":") + std::to_string(__LINE__) +      \
        ":" + std::string(__func__) + ":"

namespace aare {

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
    Cluster(int cluster_sizeX_, int cluster_sizeY_,
            Dtype dt_ = Dtype(typeid(int32_t)))
        : cluster_sizeX(cluster_sizeX_), cluster_sizeY(cluster_sizeY_),
          dt(dt_) {
        m_data = new std::byte[cluster_sizeX * cluster_sizeY * dt.bytes()]{};
    }
    Cluster() : Cluster(3, 3) {}
    Cluster(const Cluster &other)
        : Cluster(other.cluster_sizeX, other.cluster_sizeY, other.dt) {
        if (this == &other)
            return;
        x = other.x;
        y = other.y;
        memcpy(m_data, other.m_data, other.bytes());
    }
    Cluster &operator=(const Cluster &other) {
        if (this == &other)
            return *this;
        this->~Cluster();
        new (this) Cluster(other);
        return *this;
    }
    Cluster(Cluster &&other) noexcept
        : cluster_sizeX(other.cluster_sizeX),
          cluster_sizeY(other.cluster_sizeY), x(other.x), y(other.y),
          dt(other.dt), m_data(other.m_data) {
        other.m_data = nullptr;
        other.dt = Dtype(Dtype::TypeIndex::ERROR);
    }
    ~Cluster() { delete[] m_data; }
    template <typename T> T get(int idx) {
        (sizeof(T) == dt.bytes())
            ? 0
            : throw std::invalid_argument("[ERROR] Type size mismatch");
        return *reinterpret_cast<T *>(m_data + idx * dt.bytes());
    }
    template <typename T> auto set(int idx, T val) {
        (sizeof(T) == dt.bytes())
            ? 0
            : throw std::invalid_argument("[ERROR] Type size mismatch");
        return memcpy(m_data + idx * dt.bytes(), &val, (size_t)dt.bytes());
    }
    // auto x() const { return x; }
    // auto y() const { return y; }
    // auto x(int16_t x_) { return x = x_; }
    // auto y(int16_t y_) { return y = y_; }

    template <typename T> std::string to_string() const {
        (sizeof(T) == dt.bytes())
            ? 0
            : throw std::invalid_argument("[ERROR] Type size mismatch");
        std::string s = "x: " + std::to_string(x) + " y: " + std::to_string(y) +
                        "\nm_data: [";
        for (int i = 0; i < cluster_sizeX * cluster_sizeY; i++) {
            s += std::to_string(
                     *reinterpret_cast<T *>(m_data + i * dt.bytes())) +
                 " ";
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
    auto end() const {
        return m_data + cluster_sizeX * cluster_sizeY * dt.bytes();
    }
    std::byte *data() { return m_data; }
};

/**
 * @brief header contained in parts of frames
 */
struct DetectorHeader {
    uint64_t frameNumber;
    uint32_t expLength;
    uint32_t packetNumber;
    uint64_t bunchId;
    uint64_t timestamp;
    uint16_t modId;
    uint16_t row;
    uint16_t column;
    uint16_t reserved;
    uint32_t debug;
    uint16_t roundRNumber;
    uint8_t detType;
    uint8_t version;
    std::array<uint8_t, 64> packetMask;
    std::string to_string() {
        std::string packetMaskStr = "[";
        for (auto &i : packetMask) {
            packetMaskStr += std::to_string(i) + ", ";
        }
        packetMaskStr += "]";

        return "frameNumber: " + std::to_string(frameNumber) + "\n" +
               "expLength: " + std::to_string(expLength) + "\n" +
               "packetNumber: " + std::to_string(packetNumber) + "\n" +
               "bunchId: " + std::to_string(bunchId) + "\n" +
               "timestamp: " + std::to_string(timestamp) + "\n" +
               "modId: " + std::to_string(modId) + "\n" +
               "row: " + std::to_string(row) + "\n" +
               "column: " + std::to_string(column) + "\n" +
               "reserved: " + std::to_string(reserved) + "\n" +
               "debug: " + std::to_string(debug) + "\n" +
               "roundRNumber: " + std::to_string(roundRNumber) + "\n" +
               "detType: " + std::to_string(detType) + "\n" +
               "version: " + std::to_string(version) + "\n" +
               "packetMask: " + packetMaskStr + "\n";
    }
};

template <typename T> struct t_xy {
    T row;
    T col;
    bool operator==(const t_xy &other) const {
        return row == other.row && col == other.col;
    }
    bool operator!=(const t_xy &other) const { return !(*this == other); }
    std::string to_string() const {
        return "{ x: " + std::to_string(row) + " y: " + std::to_string(col) +
               " }";
    }
};
using xy = t_xy<uint32_t>;

using dynamic_shape = std::vector<int64_t>;

//TODO! Can we uniform enums between the libraries?
enum class DetectorType {
    Jungfrau,
    Eiger,
    Mythen3,
    Moench,
    Moench03,
    Moench03_old,
    ChipTestBoard,
    Unknown
};

enum class TimingMode { Auto, Trigger };
enum class FrameDiscardPolicy { NoDiscard, Discard, DiscardPartial };

template <class T> T StringTo(const std::string &arg) { return T(arg); }

template <class T> std::string ToString(T arg) { return T(arg); }

template <> DetectorType StringTo(const std::string & /*name*/);
template <> std::string ToString(DetectorType arg);

template <> TimingMode StringTo(const std::string & /*mode*/);

template <> FrameDiscardPolicy StringTo(const std::string & /*mode*/);

using DataTypeVariants = std::variant<uint16_t, uint32_t>;

} // namespace aare