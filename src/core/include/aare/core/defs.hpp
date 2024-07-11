#pragma once

#include "aare/core/Dtype.hpp"
#include "aare/utils/logger.hpp"

#include <array>
#include <stdexcept>

#include <cassert>
#include <cstdint>
#include <cstring>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

namespace aare {


/**
 * @brief header contained in parts of frames
 */
struct sls_detector_header {
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

        return "frameNumber: " + std::to_string(frameNumber) + "\n" + "expLength: " + std::to_string(expLength) + "\n" +
               "packetNumber: " + std::to_string(packetNumber) + "\n" + "bunchId: " + std::to_string(bunchId) + "\n" +
               "timestamp: " + std::to_string(timestamp) + "\n" + "modId: " + std::to_string(modId) + "\n" +
               "row: " + std::to_string(row) + "\n" + "column: " + std::to_string(column) + "\n" +
               "reserved: " + std::to_string(reserved) + "\n" + "debug: " + std::to_string(debug) + "\n" +
               "roundRNumber: " + std::to_string(roundRNumber) + "\n" + "detType: " + std::to_string(detType) + "\n" +
               "version: " + std::to_string(version) + "\n" + "packetMask: " + packetMaskStr + "\n";
    }
};

template <typename T> struct t_xy {
    T row;
    T col;
    bool operator==(const t_xy &other) const { return row == other.row && col == other.col; }
    bool operator!=(const t_xy &other) const { return !(*this == other); }
    std::string to_string() const { return "{ x: " + std::to_string(row) + " y: " + std::to_string(col) + " }"; }
};
using xy = t_xy<uint32_t>;

using dynamic_shape = std::vector<int64_t>;

enum class DetectorType { Jungfrau, Eiger, Mythen3, Moench, ChipTestBoard, Unknown };

enum class TimingMode { Auto, Trigger };

template <class T> T StringTo(const std::string &arg) { return T(arg); }

template <class T> std::string toString(T arg) { return T(arg); }

template <> DetectorType StringTo(const std::string & /*name*/);
template <> std::string toString(DetectorType arg);

template <> TimingMode StringTo(const std::string & /*mode*/);

using DataTypeVariants = std::variant<uint16_t, uint32_t>;

} // namespace aare