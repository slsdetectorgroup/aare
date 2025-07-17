#pragma once

#include "aare/Dtype.hpp"

#include <array>
#include <cassert>
#include <cstdint>
#include <cstring>
#include <stdexcept>
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

#ifdef AARE_CUSTOM_ASSERT
#define AARE_ASSERT(expr)                                                      \
    if (expr) {                                                                \
    } else                                                                     \
        aare::assert_failed(LOCATION + " Assertion failed: " + #expr + "\n");
#else
#define AARE_ASSERT(cond)                                                      \
    do {                                                                       \
        (void)sizeof(cond);                                                    \
    } while (0)
#endif

namespace aare {

inline constexpr size_t bits_per_byte = 8;

void assert_failed(const std::string &msg);

class DynamicCluster {
  public:
    int cluster_sizeX;
    int cluster_sizeY;
    int16_t x;
    int16_t y;
    Dtype dt; // 4 bytes

  private:
    std::byte *m_data;

  public:
    DynamicCluster(int cluster_sizeX_, int cluster_sizeY_,
                   Dtype dt_ = Dtype(typeid(int32_t)))
        : cluster_sizeX(cluster_sizeX_), cluster_sizeY(cluster_sizeY_),
          dt(dt_) {
        m_data = new std::byte[cluster_sizeX * cluster_sizeY * dt.bytes()]{};
    }
    DynamicCluster() : DynamicCluster(3, 3) {}
    DynamicCluster(const DynamicCluster &other)
        : DynamicCluster(other.cluster_sizeX, other.cluster_sizeY, other.dt) {
        if (this == &other)
            return;
        x = other.x;
        y = other.y;
        memcpy(m_data, other.m_data, other.bytes());
    }
    DynamicCluster &operator=(const DynamicCluster &other) {
        if (this == &other)
            return *this;
        this->~DynamicCluster();
        new (this) DynamicCluster(other);
        return *this;
    }
    DynamicCluster(DynamicCluster &&other) noexcept
        : cluster_sizeX(other.cluster_sizeX),
          cluster_sizeY(other.cluster_sizeY), x(other.x), y(other.y),
          dt(other.dt), m_data(other.m_data) {
        other.m_data = nullptr;
        other.dt = Dtype(Dtype::TypeIndex::ERROR);
    }
    ~DynamicCluster() { delete[] m_data; }
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
        return memcpy(m_data + idx * dt.bytes(), &val, dt.bytes());
    }

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

struct ROI {
    ssize_t xmin{};
    ssize_t xmax{};
    ssize_t ymin{};
    ssize_t ymax{};

    ssize_t height() const { return ymax - ymin; }
    ssize_t width() const { return xmax - xmin; }
    bool contains(ssize_t x, ssize_t y) const {
        return x >= xmin && x < xmax && y >= ymin && y < ymax;
    }
};

using dynamic_shape = std::vector<ssize_t>;

// TODO! Can we uniform enums between the libraries?

/**
 * @brief Enum class to identify different detectors.
 * The values are the same as in slsDetectorPackage
 * Different spelling to avoid confusion with the slsDetectorPackage
 */
enum class DetectorType {
    // Standard detectors match the enum values from slsDetectorPackage
    Generic,
    Eiger,
    Gotthard,
    Jungfrau,
    ChipTestBoard,
    Moench,
    Mythen3,
    Gotthard2,
    Xilinx_ChipTestBoard,

    // Additional detectors used for defining processing. Variants of the
    // standard ones.
    Moench03 = 100,
    Moench03_old,
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

constexpr uint16_t ADC_MASK = 0x3FFF; // used to mask out the gain bits in Jungfrau

} // namespace aare