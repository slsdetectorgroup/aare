#pragma once

#include "aare/RemapDefs.hpp"

namespace aare::remap::format {
static inline std::string toString(defs::Rotation);
static inline std::string toString(defs::SensorPixelGeometry const &g);
static inline std::string toString(defs::GroupStrixelGeometry const &g);
static inline std::string toString(defs::GroupConfig const &c);
inline std::ostream &operator<<(std::ostream &os, defs::GroupConfig const &c);
} // namespace aare::remap::format