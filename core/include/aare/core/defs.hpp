#pragma once

#include <array>
#include <stdexcept>

#include <cstdint>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

namespace aare {

struct Cluster {
    int16_t x;
    int16_t y;
    std::array<int32_t, 9> data;
    std::string to_string() const {
        std::string s = "x: " + std::to_string(x) + " y: " + std::to_string(y) + "\ndata: [";
        for (auto d : data) {
            s += std::to_string(d) + " ";
        }
        s += "]";
        return s;
    }
};

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
};

template <typename T> struct t_xy {
    T row;
    T col;
    bool operator==(const t_xy &other) const { return row == other.row && col == other.col; }
    bool operator!=(const t_xy &other) const { return !(*this == other); }
    std::string to_string() const { return "{ x: " + std::to_string(row) + " y: " + std::to_string(col) + " }"; }
};
typedef t_xy<uint32_t> xy;

using dynamic_shape = std::vector<ssize_t>;

enum class DetectorType { Jungfrau, Eiger, Mythen3, Moench, ChipTestBoard, Unknown };

enum class TimingMode { Auto, Trigger };

template <class T> T StringTo(const std::string &arg) { return T(arg); }

template <class T> std::string toString(T arg) { return T(arg); }

template <> DetectorType StringTo(const std::string & /*name*/);
template <> std::string toString(DetectorType arg);

template <> TimingMode StringTo(const std::string & /*mode*/);

using DataTypeVariants = std::variant<uint16_t, uint32_t>;

} // namespace aare