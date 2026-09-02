#pragma once

#include "aare/StrixelPixelRemapDefs.hpp"

namespace aare::remap::format {
// static inline std::string to_string(defs::Rotation);
// static inline std::string to_string(defs::SensorPixelGeometry const &g);
static inline std::string to_string(defs::GroupStrixelGeometry const &g);
static inline std::string to_string(defs::GroupConfig const &c);
inline std::ostream &operator<<(std::ostream &os, defs::GroupConfig const &c);
} // namespace aare::remap::format