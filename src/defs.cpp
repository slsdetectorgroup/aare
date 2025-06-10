#include "aare/defs.hpp"
#include <stdexcept>
#include <string>

#include <fmt/core.h>
namespace aare {

void assert_failed(const std::string &msg) {
    fmt::print(msg);
    exit(1);
}

/**
 * @brief Convert a DetectorType to a string
 * @param type DetectorType
 * @return string representation of the DetectorType
 */
template <> std::string ToString(DetectorType arg) {
    switch (arg) {
    case DetectorType::Generic:
        return "Generic";
    case DetectorType::Eiger:
        return "Eiger";
    case DetectorType::Gotthard:
        return "Gotthard";
    case DetectorType::Jungfrau:
        return "Jungfrau";
    case DetectorType::ChipTestBoard:
        return "ChipTestBoard";
    case DetectorType::Moench:
        return "Moench";
    case DetectorType::Mythen3:
        return "Mythen3";
    case DetectorType::Gotthard2:
        return "Gotthard2";
    case DetectorType::Xilinx_ChipTestBoard:
        return "Xilinx_ChipTestBoard";

    // Custom ones
    case DetectorType::Moench03:
        return "Moench03";
    case DetectorType::Moench03_old:
        return "Moench03_old";
    case DetectorType::Unknown:
        return "Unknown";

        // no default case to trigger compiler warning if not all
        // enum values are handled
    }
    throw std::runtime_error("Could not decode detector to string");
}

/**
 * @brief Convert a string to a DetectorType
 * @param name string representation of the DetectorType
 * @return DetectorType
 * @throw runtime_error if the string does not match any DetectorType
 */
template <> DetectorType StringTo(const std::string &arg) {
    if (arg == "Generic")
        return DetectorType::Generic;
    if (arg == "Eiger")
        return DetectorType::Eiger;
    if (arg == "Gotthard")
        return DetectorType::Gotthard;
    if (arg == "Jungfrau")
        return DetectorType::Jungfrau;
    if (arg == "ChipTestBoard")
        return DetectorType::ChipTestBoard;
    if (arg == "Moench")
        return DetectorType::Moench;
    if (arg == "Mythen3")
        return DetectorType::Mythen3;
    if (arg == "Gotthard2")
        return DetectorType::Gotthard2;
    if (arg == "Xilinx_ChipTestBoard")
        return DetectorType::Xilinx_ChipTestBoard;

    // Custom ones
    if (arg == "Moench03")
        return DetectorType::Moench03;
    if (arg == "Moench03_old")
        return DetectorType::Moench03_old;
    if (arg == "Unknown")
        return DetectorType::Unknown;

    throw std::runtime_error("Could not decode detector from: \"" + arg + "\"");
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
    throw std::runtime_error("Could not decode timing mode from: \"" + arg +
                             "\"");
}

template <> FrameDiscardPolicy StringTo(const std::string &arg) {
    if (arg == "nodiscard")
        return FrameDiscardPolicy::NoDiscard;
    if (arg == "discard")
        return FrameDiscardPolicy::Discard;
    if (arg == "discardpartial")
        return FrameDiscardPolicy::DiscardPartial;
    throw std::runtime_error("Could not decode frame discard policy from: \"" +
                             arg + "\"");
}

// template <> TimingMode StringTo<TimingMode>(std::string mode);

} // namespace aare