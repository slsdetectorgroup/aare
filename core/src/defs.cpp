#include "aare/core/defs.hpp"

namespace aare {

/**
 * @brief Convert a DetectorType to a string
 * @param type DetectorType
 * @return string representation of the DetectorType
 */
template <> std::string toString(DetectorType type) {
    switch (type) {
    case DetectorType::Jungfrau:
        return "Jungfrau";
    case DetectorType::Eiger:
        return "Eiger";
    case DetectorType::Mythen3:
        return "Mythen3";
    case DetectorType::Moench:
        return "Moench";
    case DetectorType::ChipTestBoard:
        return "ChipTestBoard";
    default:
        return "Unknown";
    }
}

/**
 * @brief Convert a string to a DetectorType
 * @param name string representation of the DetectorType
 * @return DetectorType
 * @throw runtime_error if the string does not match any DetectorType
 */
template <> DetectorType StringTo(std::string name) {
    if (name == "Jungfrau")
        return DetectorType::Jungfrau;
    else if (name == "Eiger")
        return DetectorType::Eiger;
    else if (name == "Mythen3")
        return DetectorType::Mythen3;
    else if (name == "Moench")
        return DetectorType::Moench;
    else if (name == "ChipTestBoard")
        return DetectorType::ChipTestBoard;
    else {
        throw std::runtime_error("Could not decode dector from: \"" + name + "\"");
    }
}

/**
 * @brief Convert a string to a TimingMode
 * @param mode string representation of the TimingMode
 * @return TimingMode
 * @throw runtime_error if the string does not match any TimingMode
 */
template <> TimingMode StringTo(std::string mode) {
    if (mode == "auto")
        return TimingMode::Auto;
    else if (mode == "trigger")
        return TimingMode::Trigger;
    else {
        throw std::runtime_error("Could not decode timing mode from: \"" + mode + "\"");
    }
}

// template <> TimingMode StringTo<TimingMode>(std::string mode);

} // namespace aare