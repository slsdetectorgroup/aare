
#include "aare/network_io/ZmqHeader.hpp"
#include "aare/utils/json.hpp"

#include "simdjson.h"

namespace aare {

std::string ZmqHeader::to_string() const {
    std::string s;
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
    write_digit(s, "size", size);
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

void ZmqHeader::from_string(std::string &s) { // NOLINT

    simdjson::padded_string const ps(s.c_str(), s.size());
    simdjson::ondemand::parser parser;
    simdjson::ondemand::document doc = parser.iterate(ps);
    simdjson::ondemand::object object = doc.get_object();

    for (auto field : object) {
        std::string_view const key = field.unescaped_key();

        if (key == "data") {
            data = static_cast<uint64_t>(field.value()) != 0;
        } else if (key == "jsonversion") {
            jsonversion = static_cast<uint32_t>(field.value());
        } else if (key == "dynamicRange") {
            dynamicRange = static_cast<uint32_t>(field.value());
        } else if (key == "fileIndex") {
            fileIndex = static_cast<uint64_t>(field.value());
        } else if (key == "ndetx") {
            ndetx = static_cast<uint32_t>(field.value());
        } else if (key == "ndety") {
            ndety = static_cast<uint32_t>(field.value());
        } else if (key == "npixelsx") {
            npixelsx = static_cast<uint32_t>(field.value());
        } else if (key == "npixelsy") {
            npixelsy = static_cast<uint32_t>(field.value());
        } else if (key == "size") {
            size = static_cast<uint32_t>(field.value());
        } else if (key == "acqIndex") {
            acqIndex = static_cast<uint64_t>(field.value());
        } else if (key == "frameIndex") {
            frameIndex = static_cast<uint64_t>(field.value());
        } else if (key == "progress") {
            progress = field.value().get_double();
        } else if (key == "fname") {
            std::string_view const tmp = field.value().get_string();
            fname = {tmp.begin(), tmp.end()};
        } else if (key == "frameNumber") {
            frameNumber = static_cast<uint64_t>(field.value());
        } else if (key == "expLength") {
            expLength = static_cast<uint32_t>(field.value());
        } else if (key == "packetNumber") {
            packetNumber = static_cast<uint32_t>(field.value());
        } else if (key == "detSpec1") {
            detSpec1 = static_cast<uint64_t>(field.value());
        } else if (key == "timestamp") {
            timestamp = static_cast<uint64_t>(field.value());
        } else if (key == "modId") {
            modId = static_cast<uint32_t>(field.value());
        } else if (key == "row") {
            row = static_cast<uint32_t>(field.value());
        } else if (key == "column") {
            column = static_cast<uint32_t>(field.value());
        } else if (key == "detSpec2") {
            detSpec2 = static_cast<uint32_t>(field.value());
        } else if (key == "detSpec3") {
            detSpec3 = static_cast<uint32_t>(field.value());
        } else if (key == "detSpec4") {
            detSpec4 = static_cast<uint32_t>(field.value());
        } else if (key == "detType") {
            detType = static_cast<uint32_t>(field.value());
        } else if (key == "version") {
            version = static_cast<uint32_t>(field.value());
        } else if (key == "flipRows") {
            flipRows = static_cast<int64_t>(field.value());
        } else if (key == "quad") {
            quad = static_cast<uint32_t>(field.value());
        } else if (key == "completeImage") {
            completeImage = static_cast<uint64_t>(field.value()) != 0;
        } else if (key == "addJsonHeader") {
            addJsonHeader = static_cast<std::map<std::string, std::string>>(field.value());
        } else if (key == "rx_roi") {
            rx_roi = static_cast<std::array<int, 4>>(field.value());
        }
    }
}
bool ZmqHeader::operator==(const ZmqHeader &other) const {
    return data == other.data && jsonversion == other.jsonversion && dynamicRange == other.dynamicRange &&
           fileIndex == other.fileIndex && ndetx == other.ndetx && ndety == other.ndety && npixelsx == other.npixelsx &&
           npixelsy == other.npixelsy && size == other.size && acqIndex == other.acqIndex &&
           frameIndex == other.frameIndex && progress == other.progress && fname == other.fname &&
           frameNumber == other.frameNumber && expLength == other.expLength && packetNumber == other.packetNumber &&
           detSpec1 == other.detSpec1 && timestamp == other.timestamp && modId == other.modId && row == other.row &&
           column == other.column && detSpec2 == other.detSpec2 && detSpec3 == other.detSpec3 &&
           detSpec4 == other.detSpec4 && detType == other.detType && version == other.version &&
           flipRows == other.flipRows && quad == other.quad && completeImage == other.completeImage &&
           addJsonHeader == other.addJsonHeader && rx_roi == other.rx_roi;
}

} // namespace aare