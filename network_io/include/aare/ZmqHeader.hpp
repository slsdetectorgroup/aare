#include "aare/utils/logger.hpp"
#include "simdjson.h"
#include <array>
#include <cstdint>
#include <map>
#include <string>
namespace simdjson {
/**
 * @brief cast a simdjson::ondemand::value to a std::array<int,4>
 * useful for writing rx_roi from json header
 */
template <> simdjson_inline simdjson::simdjson_result<std::array<int, 4>> simdjson::ondemand::value::get() noexcept {
    ondemand::array array;
    auto error = get_array().get(array);
    if (error) {
        return error;
    }
    std::array<int, 4> arr;
    int i = 0;
    for (auto v : array) {
        int64_t val;
        error = v.get_int64().get(val);

        if (error) {
            return error;
        }
        arr[i++] = val;
    }
    return arr;
}

/**
 * @brief cast a simdjson::ondemand::value to a uint32_t
 * adds a check for 32bit overflow
 */
template <> simdjson_inline simdjson::simdjson_result<uint32_t> simdjson::ondemand::value::get() noexcept {
    size_t val;
    auto error = get_uint64().get(val);
    if (error) {
        return error;
    }
    if (val > std::numeric_limits<uint32_t>::max()) {
        return 1;
    }
    return static_cast<uint32_t>(val);
}

} // namespace simdjson

namespace aare {

/** zmq header structure (from slsDetectorPackage)*/
struct ZmqHeader {
    /** true if incoming data, false if end of acquisition */
    bool data{true};
    uint32_t jsonversion{0};
    uint32_t dynamicRange{0};
    uint64_t fileIndex{0};
    /** number of detectors/port in x axis */
    uint32_t ndetx{0};
    /** number of detectors/port in y axis */
    uint32_t ndety{0};
    /** number of pixels/channels in x axis for this zmq socket */
    uint32_t npixelsx{0};
    /** number of pixels/channels in y axis for this zmq socket */
    uint32_t npixelsy{0};
    /** number of bytes for an image in this socket */
    uint32_t imageSize{0};
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
    int flipRows{0};
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
};
} // namespace aare