#include "aare/RemapFormat.hpp"

#include <sstream>

namespace aare::remap::format {
static inline std::string toString(defs::Rotation r) {
    return (r == defs::Rotation::Identity ? "Identity" : "Rotate180");
}

static inline std::string toString(defs::SensorPixelGeometry const &g) {
    std::ostringstream os;

    os << "SensorPixelGeometry\n"
       << " cols x rows: " << g.num_pix_x << " x " << g.num_pix_y << "\n"
       << " guardring  :\n"
       << "   x = " << g.guardring.x << "\n"
       << "   y = " << g.guardring.y << "\n";

    return os.str();
}

static inline std::string toString(defs::GroupStrixelGeometry const &g) {
    std::ostringstream os;

    os << "GroupStrixelGeometry\n"
       << " multiplicity: " << g.multiplicity << "\n"
       << " pitch       : " << g.pitch_um << " um\n";

    return os.str();
}

static inline std::string toString(defs::GroupConfig const &c) {
    std::ostringstream os;

    os << "GroupConfig\n"
       << toString(c.strixel) << "\n"
       << " placement on sensor:\n"
       << c.placement_on_sensor << "\n";

    return os.str();
}

inline std::ostream &operator<<(std::ostream &os, defs::GroupConfig const &c) {
    return os << toString(c);
}
} // namespace aare::remap::format