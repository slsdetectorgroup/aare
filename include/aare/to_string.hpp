// SPDX-License-Identifier: MPL-2.0
#pragma once

/*
 *The file to_string.hpp contains conversion to and from string for various aare
 * types. It is intentionally not a part of the public API.
 * Only include the conversion that are actually needed!!!
 */

#include "aare/defs.hpp" //enums

#include <chrono> //time conversions

namespace aare {

std::string remove_unit(std::string &str);

template <typename T>
T string_to(const std::string &t, const std::string &unit) {
    double tval{0};
    try {
        tval = std::stod(t);
    } catch (const std::invalid_argument &) {
        throw std::runtime_error("Could not convert string to time");
    }

    using std::chrono::duration;
    using std::chrono::duration_cast;
    if (unit == "ns") {
        return duration_cast<T>(duration<double, std::nano>(tval));
    } else if (unit == "us") {
        return duration_cast<T>(duration<double, std::micro>(tval));
    } else if (unit == "ms") {
        return duration_cast<T>(duration<double, std::milli>(tval));
    } else if (unit == "s" || unit.empty()) {
        return duration_cast<T>(std::chrono::duration<double>(tval));
    } else {
        throw std::runtime_error(
            "Invalid unit in conversion from string to std::chrono::duration");
    }
}

// if T has a constructor that takes a string, lets use it.
// template <class T> T string_to(const std::string &arg) { return T{arg}; }
template <typename T> T string_to(const std::string &arg) {
    std::string tmp{arg};
    auto unit = remove_unit(tmp);
    return string_to<T>(tmp, unit);
}

/**
 * @brief Convert a string to an enum value. Specialized for DetectorType,
 * UDPPortPosition, TimingMode, FrameDiscardPolicy and DACIndex.
 * @param arg string representation of the enum value
 * @return the matching enum value
 * @throw std::runtime_error if the string does not match any value
 * (std::invalid_argument for DACIndex)
 */
template <> DetectorType string_to(const std::string &arg);

template <> UDPPortPosition string_to(const std::string &arg);

template <> TimingMode string_to(const std::string &arg);

template <> FrameDiscardPolicy string_to(const std::string &arg);

template <> DACIndex string_to(const std::string &arg);

/**
 * @brief Convert a corner enum to string
 * @param arg corner enum value
 * @return string representation of the corner enum
 * @throw runtime_error if the corner value is invalid
 */
std::string to_string(const corner &arg);

} // namespace aare