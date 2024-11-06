#include "aare/defs.hpp"
#include <stdexcept>
#include <string>

namespace aare {

/**
 * @brief Convert a DetectorType to a string
 * @param type DetectorType
 * @return string representation of the DetectorType
 */
template <> std::string ToString(DetectorType arg) {
    switch (arg) {
    case DetectorType::Jungfrau:
        return "Jungfrau";
    case DetectorType::Eiger:
        return "Eiger";
    case DetectorType::Mythen3:
        return "Mythen3";
    case DetectorType::Moench:
        return "Moench";
    case DetectorType::Moench03:
        return "Moench03";
    case DetectorType::Moench03_old:
        return "Moench03_old";
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
template <> DetectorType StringTo(const std::string &arg) {
    if (arg == "Jungfrau")
        return DetectorType::Jungfrau;
    if (arg == "Eiger")
        return DetectorType::Eiger;
    if (arg == "Mythen3")
        return DetectorType::Mythen3;
    if (arg == "Moench")
        return DetectorType::Moench;
    if (arg == "Moench03")
        return DetectorType::Moench03;
    if (arg == "Moench03_old")
        return DetectorType::Moench03_old;
    if (arg == "ChipTestBoard")
        return DetectorType::ChipTestBoard;
    throw std::runtime_error("Could not decode dector from: \"" + arg + "\"");
}

/**
 * @brief Convert a string to a TimingMode
 * @param mode string representation of the TimingMode
 * @return TimingMode
 * @throw runtime_error if the string does not match any TimingMode
 */
template <> TimingMode StringTo(const std::string &arg) {
    if (arg == "auto")
        return TimingMode::Auto;
    if (arg == "trigger")
        return TimingMode::Trigger;
    throw std::runtime_error("Could not decode timing mode from: \"" + arg + "\"");
}

template <> FrameDiscardPolicy StringTo(const std::string &arg) {
    if (arg == "nodiscard")
        return FrameDiscardPolicy::NoDiscard;
    if (arg == "discard")
        return FrameDiscardPolicy::Discard;
    if (arg == "discardpartial")
        return FrameDiscardPolicy::DiscardPartial;
    throw std::runtime_error("Could not decode frame discard policy from: \"" + arg + "\"");
}

// template <> TimingMode StringTo<TimingMode>(std::string mode);

} // namespace aare