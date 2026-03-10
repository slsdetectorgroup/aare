#pragma once

#include "aare/RemapDefs.hpp"

namespace aare::remap::format {
static inline std::string toString(defs::Rotation);
static inline std::string toString(defs::SensorPixelGeometry const &g);
static inline std::string toString(defs::SensorStrixelGeometry const &g);
static inline std::string toString(defs::SensorGroupConfig const &c);
inline std::ostream &operator<<(std::ostream &os,
                                defs::SensorGroupConfig const &c);
} // namespace aare::remap::format