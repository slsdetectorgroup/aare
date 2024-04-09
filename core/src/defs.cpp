#include "aare/core/defs.hpp"

namespace aare {

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