// SPDX-License-Identifier: MPL-2.0
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

enum ReadoutMode : uint8_t {
    ANALOG_ONLY = 0,
    DIGITAL_ONLY = 1,
    ANALOG_AND_DIGITAL = 2,
    TRANSCEIVER_ONLY = 3,
    DIGITAL_AND_TRANSCEIVER = 4,
    UNKNOWN = 5
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

/**
 * @brief Enum class to define the Digital to Analog converter
 * The values are the same as in slsDetectorPackage
 */
enum DACIndex {
    DAC_0,
    DAC_1,
    DAC_2,
    DAC_3,
    DAC_4,
    DAC_5,
    DAC_6,
    DAC_7,
    DAC_8,
    DAC_9,
    DAC_10,
    DAC_11,
    DAC_12,
    DAC_13,
    DAC_14,
    DAC_15,
    DAC_16,
    DAC_17,
    VSVP,
    VTRIM,
    VRPREAMP,
    VRSHAPER,
    VSVN,
    VTGSTV,
    VCMP_LL,
    VCMP_LR,
    VCAL,
    VCMP_RL,
    RXB_RB,
    RXB_LB,
    VCMP_RR,
    VCP,
    VCN,
    VISHAPER,
    VTHRESHOLD,
    IO_DELAY,
    VREF_DS,
    VOUT_CM,
    VIN_CM,
    VREF_COMP,
    VB_COMP,
    VDD_PROT,
    VIN_COM,
    VREF_PRECH,
    VB_PIXBUF,
    VB_DS,
    VREF_H_ADC,
    VB_COMP_FE,
    VB_COMP_ADC,
    VCOM_CDS,
    VREF_RSTORE,
    VB_OPA_1ST,
    VREF_COMP_FE,
    VCOM_ADC1,
    VREF_L_ADC,
    VREF_CDS,
    VB_CS,
    VB_OPA_FD,
    VCOM_ADC2,
    VCASSH,
    VTH2,
    VRSHAPER_N,
    VIPRE_OUT,
    VTH3,
    VTH1,
    VICIN,
    VCAS,
    VCAL_N,
    VIPRE,
    VCAL_P,
    VDCSH,
    VBP_COLBUF,
    VB_SDA,
    VCASC_SFP,
    VIPRE_CDS,
    IBIAS_SFP,
    ADC_VPP,
    HIGH_VOLTAGE,
    TEMPERATURE_ADC,
    TEMPERATURE_FPGA,
    TEMPERATURE_FPGAEXT,
    TEMPERATURE_10GE,
    TEMPERATURE_DCDC,
    TEMPERATURE_SODL,
    TEMPERATURE_SODR,
    TEMPERATURE_FPGA2,
    TEMPERATURE_FPGA3,
    TRIMBIT_SCAN,
    V_POWER_A = 100,
    V_POWER_B = 101,
    V_POWER_C = 102,
    V_POWER_D = 103,
    V_POWER_IO = 104,
    V_POWER_CHIP = 105,
    I_POWER_A = 106,
    I_POWER_B = 107,
    I_POWER_C = 108,
    I_POWER_D = 109,
    I_POWER_IO = 110,
    V_LIMIT = 111,
    SLOW_ADC0 = 1000,
    SLOW_ADC1,
    SLOW_ADC2,
    SLOW_ADC3,
    SLOW_ADC4,
    SLOW_ADC5,
    SLOW_ADC6,
    SLOW_ADC7,
    SLOW_ADC_TEMP
};

// helper pair class to easily expose in python
template <typename T1, typename T2> struct Sum_index_pair {
    T1 sum;
    T2 index;
};

enum class corner : int {
    cTopLeft = 0,
    cTopRight = 1,
    cBottomLeft = 2,
    cBottomRight = 3
};

enum class TimingMode { Auto, Trigger };
enum class FrameDiscardPolicy { NoDiscard, Discard, DiscardPartial };

using DataTypeVariants = std::variant<uint16_t, uint32_t>;

constexpr uint16_t ADC_MASK =
    0x3FFF; // used to mask out the gain bits in Jungfrau

class BitOffset {
    uint8_t m_offset{};

  public:
    BitOffset() = default;
    explicit BitOffset(uint32_t offset);
    uint8_t value() const { return m_offset; }
    bool operator==(const BitOffset &other) const;
    bool operator<(const BitOffset &other) const;
};

} // namespace aare