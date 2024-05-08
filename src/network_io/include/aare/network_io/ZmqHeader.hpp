#pragma once
#include "aare/core/Frame.hpp"
#include "aare/utils/logger.hpp"

#include "simdjson.h"
#include <array>
#include <cstdint>
#include <map>
#include <string>
namespace simdjson {
// template <typename T, int N> std::array

// template <int N> simdjson_inline simdjson::simdjson_result<std::array<int, N>> simdjson::ondemand::value::get()
// noexcept {
//     ondemand::array array;
//     auto error = get_array().get(array);
//     if (error) {
//         return error;
//     }
//     std::array<int, N> arr{};
//     int i = 0;
//     for (auto v : array) {
//         int64_t val = 0;
//         error = v.get_int64().get(val);

//         if (error) {
//             return error;
//         }
//         arr[i++] = static_cast<int>(val);
//     }
//     return arr;
// }

/**
 * @brief cast a simdjson::ondemand::value to a uint32_t
 * adds a check for 32bit overflow
 */
template <> simdjson_inline simdjson::simdjson_result<uint32_t> simdjson::ondemand::value::get() noexcept {
    size_t val = 0;
    auto error = get_uint64().get(val);
    if (error) {
        return error;
    }
    if (val > std::numeric_limits<uint32_t>::max()) {
        return 1;
    }
    return static_cast<uint32_t>(val);
}

/**
 * @brief cast a simdjson::ondemand::value to a std::map<std::string, std::string>
 */
template <>
simdjson_inline simdjson::simdjson_result<std::map<std::string, std::string>>
simdjson::ondemand::value::get() noexcept {
    std::map<std::string, std::string> map;
    ondemand::object obj;
    auto error = get_object().get(obj);
    if (error) {
        return error;
    }
    for (auto field : obj) {
        simdjson::ondemand::raw_json_string tmp;
        error = field.key().get(tmp);
        if (error) {
            return error;
        }
        error = field.value().get(tmp);
        if (error) {
            return error;
        }
        std::string_view const key_view = field.unescaped_key();
        std::string const key_str(key_view.data(), key_view.size());
        std::string_view const value_view = field.value().get_string();
        map[key_str] = {value_view.data(), value_view.size()};
    }
    return map;
}

} // namespace simdjson

namespace aare {
/** zmq header structure (from slsDetectorPackage)*/
struct ZmqHeader {
    /** true if incoming data, false if end of acquisition */
    bool data{true};
    uint32_t jsonversion{0};
    uint32_t bitmode{0};
    uint64_t fileIndex{0};
    /** number of detectors/port*/
    t_xy<uint32_t> detshape{0, 0};
    /** number of pixels/channels for this zmq socket */
    t_xy<uint32_t> shape{0, 0};
    /** number of bytes for an image in this socket */
    uint32_t size{0};
    /** frame number from detector */
    uint64_t acqIndex{0};
    /** frame index (starting at 0 for each acquisition) */
    uint64_t frameIndex{0};
    /** progress in percentage */
    double progress{0};
    /** file name prefix */
    std::string fname;
    /** header from detector */
    uint64_t frameNumber{0};
    uint32_t expLength{0};
    uint32_t packetNumber{0};
    uint64_t detSpec1{0};
    uint64_t timestamp{0};
    uint16_t modId{0};
    uint16_t row{0};
    uint16_t column{0};
    uint16_t detSpec2{0};
    uint32_t detSpec3{0};
    uint16_t detSpec4{0};
    uint8_t detType{0};
    uint8_t version{0};
    /** if rows of image should be flipped */
    int64_t flipRows{0};
    /** quad type (eiger hardware specific) */
    uint32_t quad{0};
    /** true if complete image, else missing packets */
    bool completeImage{false};
    /** additional json header */
    std::map<std::string, std::string> addJsonHeader;
    /** (xmin, xmax, ymin, ymax) roi only in files written */
    std::array<int, 4> rx_roi{};

    /** serialize struct to json string */
    std::string to_string() const;
    void from_string(std::string &s);
    // compare operator
    bool operator==(const ZmqHeader &other) const;
};
/**
 * @brief cast a simdjson::ondemand::value to a std::array<int,4>
 * useful for writing rx_roi from json header
 */
template <typename T, int N, typename SIMDJSON_VALUE> std::array<T, N> simd_convert_array(SIMDJSON_VALUE field) {
    simdjson::ondemand::array simd_array;
    auto err = field.value().get_array().get(simd_array);
    if (err)
        throw std::runtime_error("error converting simdjson::ondemand::value to simdjson::ondemend::array");
    std::array<T, N> arr{};
    int i = 0;
    for (auto v : simd_array) {
        int64_t tmp{};
        err = v.get(tmp);
        if (err)
            throw std::runtime_error("error converting simdjson::ondemand::value");
        arr[i++] = tmp;
    }
    return arr;
}

} // namespace aare