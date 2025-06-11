#include "aare/defs.hpp"
#include <chrono>
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
 * @brief Convert a TimingMode to a string
 * @param type TimingMode
 * @return string representation of the TimingMode
 */
template <> std::string ToString(TimingMode arg) {
    switch (arg) {
    case TimingMode::Auto:
        return "Auto";
    case TimingMode::Trigger:
        return "Trigger";

        // no default case to trigger compiler warning if not all
        // enum values are handled
    }
    throw std::runtime_error("Could not decode timing mode to string");
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

/**
 * @brief Convert a FrameDiscardPolicy to a string
 * @param type FrameDiscardPolicy
 * @return string representation of the FrameDiscardPolicy
 */
template <> std::string ToString(FrameDiscardPolicy arg) {
    switch (arg) {
    case FrameDiscardPolicy::NoDiscard:
        return "nodiscard";
    case FrameDiscardPolicy::Discard:
        return "discard";
    case FrameDiscardPolicy::DiscardPartial:
        return "discardpartial";

        // no default case to trigger compiler warning if not all
        // enum values are handled
    }
    throw std::runtime_error("Could not decode frame discard policy to string");
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

/**
 * @brief Convert a BurstMode to a string
 * @param type BurstMode
 * @return string representation of the BurstMode
 */
template <> std::string ToString(BurstMode arg) {
    switch (arg) {
    case BurstMode::Burst_Interal:
        return "burst_internal";
    case BurstMode::Burst_External:
        return "burst_external";
    case BurstMode::Continuous_Internal:
        return "continuous_internal";
    case BurstMode::Continuous_External:
        return "continuous_external";
    }
    throw std::runtime_error("Could not decode burst mode to string");
}

template <> BurstMode StringTo(const std::string &arg) {
    if (arg == "burst_internal")
        return BurstMode::Burst_Interal;
    if (arg == "burst_external")
        return BurstMode::Burst_External;
    if (arg == "continuous_internal")
        return BurstMode::Continuous_Internal;
    if (arg == "continuous_external")
        return BurstMode::Continuous_External;
    throw std::runtime_error("Could not decode burst mode from: \"" + arg +
                             "\"");
}

/**
 * @brief Convert a ScanParameters to a string
 * @param type ScanParameters
 * @return string representation of the ScanParameters
 */
template <> std::string ToString(ScanParameters arg) {
    std::ostringstream oss;
    oss << '[';
    if (arg.enabled()) {
        oss << "enabled" << std::endl
            << "dac " << arg.dac() << std::endl
            << "start " << arg.start() << std::endl
            << "stop " << arg.stop() << std::endl
            << "step " << arg.step()
            << std::endl
            //<< "settleTime "
            // << ToString(std::chrono::nanoseconds{arg.dacSettleTime_ns})
            << std::endl;
    } else {
        oss << "disabled";
    }
    oss << ']';
    return oss.str();
}

std::ostream &operator<<(std::ostream &os, const ScanParameters &r) {
    return os << ToString(r);
}

/**
 * @brief Convert a ROI to a string
 * @param type ROI
 * @return string representation of the ROI
 */
template <> std::string ToString(ROI arg) {
    std::ostringstream oss;
    oss << '[' << arg.xmin << ", " << arg.xmax;
    if (arg.ymin != -1 || arg.ymax != -1) {
        oss << ", " << arg.ymin << ", " << arg.ymax;
    }
    oss << ']';
    return oss.str();
}

std::ostream &operator<<(std::ostream &os, const ROI &roi) {
    return os << ToString(roi);
}

std::string RemoveUnit(std::string &str) {
    auto it = str.begin();
    while (it != str.end()) {
        if (std::isalpha(*it))
            break;
        ++it;
    }
    auto pos = it - str.begin();
    auto unit = str.substr(pos);
    str.erase(it, end(str));
    return unit;
}

// template <> TimingMode StringTo<TimingMode>(std::string mode);

} // namespace aare