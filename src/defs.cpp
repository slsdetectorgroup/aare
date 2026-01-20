// SPDX-License-Identifier: MPL-2.0
#include "aare/defs.hpp"
#include <stdexcept>
#include <string>

#include <fmt/core.h>
namespace aare {

void assert_failed(const std::string &msg) {
    fmt::print(msg);
    exit(1);
}

// /**
//  * @brief Convert a DetectorType to a string
//  * @param type DetectorType
//  * @return string representation of the DetectorType
//  */
// template <> std::string ToString(DetectorType arg) {
//     switch (arg) {
//     case DetectorType::Generic:
//         return "Generic";
//     case DetectorType::Eiger:
//         return "Eiger";
//     case DetectorType::Gotthard:
//         return "Gotthard";
//     case DetectorType::Jungfrau:
//         return "Jungfrau";
//     case DetectorType::ChipTestBoard:
//         return "ChipTestBoard";
//     case DetectorType::Moench:
//         return "Moench";
//     case DetectorType::Mythen3:
//         return "Mythen3";
//     case DetectorType::Gotthard2:
//         return "Gotthard2";
//     case DetectorType::Xilinx_ChipTestBoard:
//         return "Xilinx_ChipTestBoard";

//     // Custom ones
//     case DetectorType::Moench03:
//         return "Moench03";
//     case DetectorType::Moench03_old:
//         return "Moench03_old";
//     case DetectorType::Unknown:
//         return "Unknown";

//         // no default case to trigger compiler warning if not all
//         // enum values are handled
//     }
//     throw std::runtime_error("Could not decode detector to string");
// }


BitOffset::BitOffset(uint32_t offset){
    if (offset>7)
        throw std::runtime_error(fmt::format("{} BitOffset needs to be <8: Called with {}", LOCATION, offset));

    m_offset = static_cast<uint8_t>(offset);

}

bool BitOffset::operator==(const BitOffset& other) const {
        return m_offset == other.m_offset;
    }

bool BitOffset::operator<(const BitOffset& other) const {
        return m_offset < other.m_offset;
    }



} // namespace aare