// SPDX-License-Identifier: MPL-2.0

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
    } catch (const std::invalid_argument &e) {
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
 * @brief Convert a string to DetectorType
 * @param name string representation of the DetectorType
 * @return DetectorType
 * @throw runtime_error if the string does not match any DetectorType
 */
template <> DetectorType string_to(const std::string &arg);

/**
 * @brief Convert a string to TimingMode
 * @param mode string representation of the TimingMode
 * @return TimingMode
 * @throw runtime_error if the string does not match any TimingMode
 */
template <> TimingMode string_to(const std::string &arg);

/**
 * @brief Convert a string to FrameDiscardPolicy
 * @param mode string representation of the FrameDiscardPolicy
 * @return FrameDiscardPolicy
 * @throw runtime_error if the string does not match any FrameDiscardPolicy
 */
template <> FrameDiscardPolicy string_to(const std::string &arg);

/**
 * @brief Convert a string to a DACIndex
 * @param arg string representation of the dacIndex
 * @return DACIndex
 * @throw invalid argument error if the string does not match any DACIndex
 */
template <> DACIndex string_to(const std::string &arg);

} // namespace aare