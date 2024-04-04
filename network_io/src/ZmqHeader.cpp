
#include "aare/ZmqHeader.hpp"

#include "simdjson.h"

using namespace simdjson;

// helper functions to write json
// append to string for better performance (not tested)

/**
 * @brief write a digit to a string
 * takes key and value and outputs->"key": value,
 * @tparam T type of value (int, uint32_t, ...)
 * @param s string to append to
 * @param key key to write
 * @param value value to write
 * @return void
 * @note
 * - can't use concepts here because we are using c++17
 */
template <typename T> void write_digit(std::string &s, const std::string &key, const T &value) {
    s += "\"";
    s += key;
    s += "\": ";
    s += std::to_string(value);
    s += ", ";
}
void write_str(std::string &s, const std::string &key, const std::string &value) {
    s += "\"";
    s += key;
    s += "\": \"";
    s += value;
    s += "\", ";
}
void write_map(std::string &s, const std::string &key, const std::map<std::string, std::string> &value) {
    s += "\"";
    s += key;
    s += "\": {";
    for (auto &kv : value) {
        write_str(s, kv.first, kv.second);
    }
    s += "}, ";
}
void write_array(std::string &s, const std::string &key, const std::array<int, 4> &value) {
    s += "\"";
    s += key;
    s += "\": [";
    s += std::to_string(value[0]);
    s += ", ";
    s += std::to_string(value[1]);
    s += ", ";
    s += std::to_string(value[2]);
    s += ", ";
    s += std::to_string(value[3]);
    s += "], ";
}

namespace aare {

std::string ZmqHeader::to_string() const {
    std::string s = "";
    s.reserve(1024);
    s += "{";
    write_digit(s, "data", data ? 1 : 0);
    write_digit(s, "jsonversion", jsonversion);
    write_digit(s, "dynamicRange", dynamicRange);
    write_digit(s, "fileIndex", fileIndex);
    write_digit(s, "ndetx", ndetx);
    write_digit(s, "ndety", ndety);
    write_digit(s, "npixelsx", npixelsx);
    write_digit(s, "npixelsy", npixelsy);
    write_digit(s, "imageSize", imageSize);
    write_digit(s, "acqIndex", acqIndex);
    write_digit(s, "frameIndex", frameIndex);
    write_digit(s, "progress", progress);
    write_str(s, "fname", fname);
    write_digit(s, "frameNumber", frameNumber);
    write_digit(s, "expLength", expLength);
    write_digit(s, "packetNumber", packetNumber);
    write_digit(s, "detSpec1", detSpec1);
    write_digit(s, "timestamp", timestamp);
    write_digit(s, "modId", modId);
    write_digit(s, "row", row);
    write_digit(s, "column", column);
    write_digit(s, "detSpec2", detSpec2);
    write_digit(s, "detSpec3", detSpec3);
    write_digit(s, "detSpec4", detSpec4);
    write_digit(s, "detType", detType);
    write_digit(s, "version", version);
    write_digit(s, "flipRows", flipRows);
    write_digit(s, "quad", quad);
    write_digit(s, "completeImage", completeImage ? 1 : 0);
    write_map(s, "addJsonHeader", addJsonHeader);
    write_array(s, "rx_roi", rx_roi);
    // remove last comma
    s.pop_back();
    s.pop_back();

    s += "}";
    return s;
}

void ZmqHeader::from_string(std::string &s) {

    simdjson::padded_string ps(s.c_str(), s.size());
    ondemand::parser parser;
    ondemand::document doc = parser.iterate(ps);
    ondemand::object object = doc.get_object();

    for (auto field : object) {
        std::string_view key = field.unescaped_key();

        if (key == "data") {
            data = uint64_t(field.value()) ? true : false;
        } else if (key == "jsonversion") {
            jsonversion = uint32_t(field.value());
        } else if (key == "dynamicRange") {
            dynamicRange = uint32_t(field.value());
        } else if (key == "fileIndex") {
            fileIndex = uint64_t(field.value());
        } else if (key == "ndetx") {
            ndetx = uint32_t(field.value());
        } else if (key == "ndety") {
            ndety = uint32_t(field.value());
        } else if (key == "npixelsx") {
            npixelsx = uint32_t(field.value());
        } else if (key == "npixelsy") {
            npixelsy = uint32_t(field.value());
        } else if (key == "imageSize") {
            imageSize = uint32_t(field.value());
        } else if (key == "acqIndex") {
            acqIndex = uint64_t(field.value());
        } else if (key == "frameIndex") {
            frameIndex = uint64_t(field.value());
        } else if (key == "progress") {
            progress = field.value().get_double();
        } else if (key == "fname") {
            std::string_view tmp = field.value().get_string();
            fname = {tmp.begin(), tmp.end()};
        } else if (key == "frameNumber") {
            frameNumber = uint64_t(field.value());
        } else if (key == "expLength") {
            expLength = uint32_t(field.value());
        } else if (key == "packetNumber") {
            packetNumber = uint32_t(field.value());
        } else if (key == "detSpec1") {
            detSpec1 = uint64_t(field.value());
        } else if (key == "timestamp") {
            timestamp = uint64_t(field.value());
        } else if (key == "modId") {
            modId = uint32_t(field.value());
        } else if (key == "row") {
            row = uint32_t(field.value());
        } else if (key == "column") {
            column = uint32_t(field.value());
        } else if (key == "detSpec2") {
            detSpec2 = uint32_t(field.value());
        } else if (key == "detSpec3") {
            detSpec3 = uint32_t(field.value());
        } else if (key == "detSpec4") {
            detSpec4 = uint32_t(field.value());
        } else if (key == "detType") {
            detType = uint32_t(field.value());
        } else if (key == "version") {
            version = uint32_t(field.value());
        } else if (key == "flipRows") {
            flipRows = uint32_t(field.value());
        } else if (key == "quad") {
            quad = uint32_t(field.value());
        } else if (key == "completeImage") {
            completeImage = uint64_t(field.value()) ? true : false;
        } else if (key == "addJsonHeader") {
            for (auto field2 : field.value().get_object()) {
                simdjson::ondemand::raw_json_string tmp;
                auto error = field2.key().get(tmp);
                std::string key2(tmp.raw());
                std::string val;
                error = field2.value().get_string(val);
                addJsonHeader[key2] = std::string(val);
            }
        } else if (key == "rx_roi") {
            rx_roi = std::array<int, 4>(field.value());
        }
    }
}

} // namespace aare