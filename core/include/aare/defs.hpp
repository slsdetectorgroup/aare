#pragma once

#include <array>
#include <vector>
#include <sys/types.h>
#include <string_view>
#include <string>
#include <stdexcept>
#include <fmt/format.h>
#include <variant>

typedef struct {
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
    uint8_t packetMask[64];
}__attribute__((packed)) sls_detector_header;

struct xy {
    int row;
    int col;
};

// using image_shape = std::array<ssize_t, 2>;
using dynamic_shape = std::vector<ssize_t>;

enum class DetectorType { Jungfrau, Eiger, Mythen3, Moench,ChipTestBoard };

enum class TimingMode {Auto, Trigger};

template<class T> 
T StringTo(std::string sv){
    return T(sv);
}

template<class T> 
std::string toString(T sv){
    return T(sv);
}

template <> DetectorType StringTo(std::string);
template <> std::string toString(DetectorType type);


template <> TimingMode StringTo(std::string);

using DataTypeVariants = std::variant<uint16_t, uint32_t>;


struct RawFileConfig {
    int module_gap_row{};
    int module_gap_col{};

    bool operator==(const RawFileConfig &other) const {
        if (module_gap_col != other.module_gap_col)
            return false;
        if (module_gap_row != other.module_gap_row)
            return false;
        return true;
    }
};

const char little_endian_char = '<';
const char big_endian_char = '>';
const char no_endian_char = '|';

const std::array<char, 3> endian_chars = {little_endian_char, big_endian_char, no_endian_char};
const std::array<char, 4> numtype_chars = {'f', 'i', 'u', 'c'};