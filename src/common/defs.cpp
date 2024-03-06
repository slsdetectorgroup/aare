#include "common/defs.hpp"

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
    else {
        auto msg = fmt::format("Could not decode dector from: \"{}\"", name);
        throw std::runtime_error(msg);
    }
}

template <> TimingMode StringTo(std::string mode){
    if (mode == "auto")
        return TimingMode::Auto;
    else if(mode == "trigger")
        return TimingMode::Trigger;
    else{
        auto msg = fmt::format("Could not decode timing mode from: \"{}\"", mode);
        throw std::runtime_error(msg);
    }
}

// template <> TimingMode StringTo<TimingMode>(std::string mode);