#pragma once

#include <array>
#include <vector>
#include <sys/types.h>
#include <string_view>
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

enum class DetectorType { Jungfrau, Eiger, Mythen3, Moench };

enum class TimingMode {Auto, Trigger};

template<class T> 
T StringTo(std::string sv){
    return T(sv);
}

template <> DetectorType StringTo(std::string);

template <> TimingMode StringTo(std::string);

using DataTypeVariants = std::variant<uint16_t, uint32_t>;

